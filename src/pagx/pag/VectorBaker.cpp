// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 5c VectorBaker + Phase 6 PaintBaker — translates a PAGX layer's
// `contents` (vector of pagx::Element*) into PAGDocument's VectorPayload.
// Phase 5c covered nine geometry / modifier element types. Phase 6 adds
// Fill / Stroke element bakers plus the ColorSource dispatch (SolidColor /
// LinearGradient / RadialGradient / ConicGradient / DiamondGradient /
// ImagePattern) that fills the inline ShapeStyleData on each painter.
// Text / TextPath / TextModifier remain in Phase 8.
//
// PAGX nodes expose plain fields (no Property<T> wrapper) plus a NaN-sentinel
// convention for "unset" positions. The baker substitutes `Point{0, 0}` for
// NaN coordinates rather than calling renderPosition() — calling that would
// require applyLayout() to have produced layoutBounds, and the Phase 3
// applyLayout perf cliff (memory: feedback_pagx_layout_perf.md) makes that
// brittle for synthetic test inputs.
#include "pagx/pag/VectorBaker.h"
#include <cmath>
#include <memory>
#include <utility>
#include <vector>
#include "pagx/nodes/ColorSource.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/Element.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Gradient.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/MergePath.h"
#include "pagx/nodes/Node.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/RoundCorner.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx/pag/BakeContext.h"
#include "pagx/pag/PAGDocument.h"
#include "pagx/pag/ResourceBaker.h"
#include "pagx/pag/TextBaker.h"
#include "renderer/ToTGFX.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/ImageCodec.h"

namespace pagx::pag {

namespace {

// PAGX uses NAN as "position not explicitly set"; the element's
// renderPosition() / renderSize() / renderOuterRadius() / ... accessors
// compute the final layout value from layoutBounds(). Bake is always
// invoked after PAGXDocument::applyLayout() (Baker.cpp enforces it via
// the LayoutNotApplied=100 preflight gate), so the render*() accessors
// always return finite values. Using the raw fields instead — as earlier
// Phase 5c/6 did — produces NaN positions and zero sizes when the PAGX
// source uses the left/top/width/height layout shorthand, which in turn
// makes the Inflater feed `tgfx::Rectangle::setSize({0,0})` and trip the
// `DEBUG_ASSERT(!rRect.rect.isEmpty())` in tgfx::OpsCompositor::drawRRect.
// Phase 11.6 fix.
tgfx::Point PointFromRenderPosition(const pagx::Point& p) {
  // `renderPosition()` always returns a finite Point post-layout; keep the
  // NaN-guard purely as a belt-and-braces for future layout bugs — zero
  // lines of behavioural cost when layout is correct.
  float x = std::isnan(p.x) ? 0.0f : p.x;
  float y = std::isnan(p.y) ? 0.0f : p.y;
  return tgfx::Point{x, y};
}

tgfx::Point SizeToPoint(const pagx::Size& s) {
  return tgfx::Point{s.width, s.height};
}

// pagx::PolystarType is { Polygon=0, Star=1 } but tgfx::PolystarType is
// { Star=0, Polygon=1 } — direct cast would flip the meaning. Map by name.
tgfx::PolystarType MapPolystarType(pagx::PolystarType t) {
  return t == pagx::PolystarType::Star ? tgfx::PolystarType::Star : tgfx::PolystarType::Polygon;
}

// pagx::TrimType { Separate=0, Continuous=1 } matches tgfx::TrimPathType
// { Separate=0, Continuous=1 } — but ToTGFX has no overload for it, so map
// explicitly to keep the dependency tight.
tgfx::TrimPathType MapTrimType(pagx::TrimType t) {
  return t == pagx::TrimType::Continuous ? tgfx::TrimPathType::Continuous
                                         : tgfx::TrimPathType::Separate;
}

std::unique_ptr<VectorElement> BakeElement(BakeContext& ctx, PAGDocument& doc,
                                           const pagx::Element* src);

std::unique_ptr<VectorElement> BakeRectangle(const pagx::Rectangle& src) {
  auto data = std::make_unique<ElementRectangleData>();
  data->position = MakeProp(PointFromRenderPosition(src.renderPosition()));
  data->size = MakeProp(SizeToPoint(src.renderSize()));
  data->roundness = MakeProp(src.roundness);
  data->reversed = src.reversed;

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::Rectangle;
  el->payload = std::move(data);
  return el;
}

std::unique_ptr<VectorElement> BakeEllipse(const pagx::Ellipse& src) {
  auto data = std::make_unique<ElementEllipseData>();
  data->position = MakeProp(PointFromRenderPosition(src.renderPosition()));
  data->size = MakeProp(SizeToPoint(src.renderSize()));
  data->reversed = src.reversed;

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::Ellipse;
  el->payload = std::move(data);
  return el;
}

std::unique_ptr<VectorElement> BakePolystar(const pagx::Polystar& src) {
  auto data = std::make_unique<ElementPolystarData>();
  data->position = MakeProp(PointFromRenderPosition(src.renderPosition()));
  data->pointCount = MakeProp(src.pointCount);
  // `renderOuterRadius()` / `renderInnerRadius()` collapse the PAGX layout
  // shorthand into explicit radii the same way LayerBuilder does.
  data->outerRadius = MakeProp(src.renderOuterRadius());
  data->innerRadius = MakeProp(src.renderInnerRadius());
  data->outerRoundness = MakeProp(src.outerRoundness);
  data->innerRoundness = MakeProp(src.innerRoundness);
  data->rotation = MakeProp(src.rotation);
  data->reversed = src.reversed;
  data->polystarType = MapPolystarType(src.type);

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::Polystar;
  el->payload = std::move(data);
  return el;
}

std::unique_ptr<VectorElement> BakePath(const pagx::Path& src) {
  auto data = std::make_unique<ElementShapePathData>();
  data->position = MakeProp(PointFromRenderPosition(src.renderPosition()));
  if (src.data != nullptr) {
    // LayerBuilder (renderer/LayerBuilder.cpp `convertPath` /
    // `getScaledPath`) pre-applies `renderScale()` to the raw path. Baker
    // bakes the already-scaled path so the two pipelines store the same
    // geometry on the wire. Without this the PSNR against rich_text /
    // path / text_path samples stays below 30 dB even after the Rectangle
    // / Ellipse / Layer matrix fixes.
    auto path = pagx::ToTGFX(*src.data);
    const float scale = src.renderScale();
    if (scale != 1.0f) {
      path.transform(tgfx::Matrix::MakeScale(scale));
    }
    data->path = MakeProp(std::move(path));
  }
  data->reversed = src.reversed;

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::ShapePath;
  el->payload = std::move(data);
  return el;
}

std::unique_ptr<VectorElement> BakeTrimPath(const pagx::TrimPath& src) {
  auto data = std::make_unique<ElementTrimPathData>();
  data->start = MakeProp(src.start);
  data->end = MakeProp(src.end);
  data->offset = MakeProp(src.offset);
  data->trimType = MapTrimType(src.type);

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::TrimPath;
  el->payload = std::move(data);
  return el;
}

std::unique_ptr<VectorElement> BakeRoundCorner(const pagx::RoundCorner& src) {
  auto data = std::make_unique<ElementRoundCornerData>();
  data->radius = MakeProp(src.radius);

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::RoundCorner;
  el->payload = std::move(data);
  return el;
}

std::unique_ptr<VectorElement> BakeMergePath(const pagx::MergePath& src) {
  auto data = std::make_unique<ElementMergePathData>();
  data->mode = pagx::ToTGFX(src.mode);

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::MergePath;
  el->payload = std::move(data);
  return el;
}

std::unique_ptr<VectorElement> BakeRepeater(const pagx::Repeater& src) {
  auto data = std::make_unique<ElementRepeaterData>();
  data->copies = MakeProp(src.copies);
  data->offset = MakeProp(src.offset);
  data->order = pagx::ToTGFX(src.order);
  data->anchor = MakeProp(tgfx::Point{src.anchor.x, src.anchor.y});
  data->position = MakeProp(tgfx::Point{src.position.x, src.position.y});
  data->rotation = MakeProp(src.rotation);
  data->scale = MakeProp(tgfx::Point{src.scale.x, src.scale.y});
  data->startAlpha = MakeProp(src.startAlpha);
  data->endAlpha = MakeProp(src.endAlpha);

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::Repeater;
  el->payload = std::move(data);
  return el;
}

std::unique_ptr<VectorElement> BakeGroup(BakeContext& ctx, PAGDocument& doc,
                                         const pagx::Group& src) {
  if (!ctx.enterVectorGroup()) {
    return nullptr;
  }
  auto data = std::make_unique<ElementVectorGroupData>();
  data->anchor = MakeProp(tgfx::Point{src.anchor.x, src.anchor.y});
  // Phase 11.6: Group inherits from LayoutNode — its authored `position`
  // may be NaN when the group is positioned through flex / left / top.
  // LayerBuilder (renderer/LayerBuilder.cpp `convertGroup`) calls
  // `renderPosition()`; Baker follows suit.
  auto renderPos = src.renderPosition();
  data->position = MakeProp(tgfx::Point{renderPos.x, renderPos.y});
  data->scale = MakeProp(tgfx::Point{src.scale.x, src.scale.y});
  data->rotation = MakeProp(src.rotation);
  data->alpha = MakeProp(src.alpha);
  data->skew = MakeProp(src.skew);
  data->skewAxis = MakeProp(src.skewAxis);

  for (auto* child : src.elements) {
    if (ctx.hasFatal()) {
      break;
    }
    auto baked = BakeElement(ctx, doc, child);
    if (baked != nullptr) {
      data->elements.push_back(std::move(baked));
    }
  }

  ctx.exitVectorGroup();

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::VectorGroup;
  el->payload = std::move(data);
  return el;
}

// ---------- PaintBaker (Phase 6) ----------
//
// PaintBaker produces the inline ShapeStyleData carried by Fill / Stroke
// element payloads. The dispatcher routes pagx::ColorSource* subtypes to
// one of six branch-specific bakers; each one touches only the fields that
// sourceType declares active, leaving the rest at PAGDocument defaults so
// the wire codec's isDefault short-circuit compresses the wrapper.
//
// Gradient-stops fallback (§F.3): the baker injects [Black@0, White@1]
// when a gradient node has no color stops, matching LayerBuilder:487-500.
//
// ImagePattern image interning: RegisterImage is called only when the PAGX
// Image node carries inline bytes. File-path-only images get
// patternImageIndex = UINT32_MAX + ImageSourceMissing=200 so the Phase 9
// Inflater can substitute a placeholder bitmap. Phase 10.5 PAGLoader will
// widen this path to actually read files.

void PopulateGradientStops(const pagx::Gradient& src, ShapeStyleData& style) {
  std::vector<tgfx::Color> colors;
  std::vector<float> positions;
  if (src.colorStops.empty()) {
    // Design doc §F.3 fallback — keeps Inflater well-formed.
    colors.push_back(tgfx::Color{0.0f, 0.0f, 0.0f, 1.0f});
    colors.push_back(tgfx::Color{1.0f, 1.0f, 1.0f, 1.0f});
    positions.push_back(0.0f);
    positions.push_back(1.0f);
  } else {
    colors.reserve(src.colorStops.size());
    positions.reserve(src.colorStops.size());
    for (const auto* stop : src.colorStops) {
      if (stop == nullptr) {
        continue;
      }
      colors.push_back(
          tgfx::Color{stop->color.red, stop->color.green, stop->color.blue, stop->color.alpha});
      positions.push_back(stop->offset);
    }
  }
  style.stopColors = MakeProp(std::move(colors));
  style.stopPositions = MakeProp(std::move(positions));
}

// Reuses the PAGX -> tgfx matrix converter from the renderer layer so
// gradient / pattern matrices follow the same semantics as pagx rendering.
tgfx::Matrix BakeMatrix(const pagx::Matrix& m) {
  return pagx::ToTGFX(m);
}

// Interns the ImagePattern's source Image into PAGDocument::images and
// returns the resulting index. Returns UINT32_MAX when the image has no
// inline bytes (Baker side loads files from disk when only a filePath is
// available; embedded Data bytes take priority when both are present).
uint32_t InternPatternImage(BakeContext& ctx, PAGDocument& doc, const pagx::Image* image) {
  if (image == nullptr) {
    return UINT32_MAX;
  }
  // Build a tgfx::Data: prefer embedded bytes, fall back to loading the file.
  std::shared_ptr<tgfx::Data> imageData;
  if (image->data != nullptr && !image->data->empty()) {
    imageData = tgfx::Data::MakeWithCopy(image->data->bytes(), image->data->size());
  } else if (!image->filePath.empty()) {
    imageData = tgfx::Data::MakeFromFile(image->filePath);
  }
  if (imageData == nullptr || imageData->empty()) {
    ctx.warn(ErrorCode::ImageSourceMissing,
             "ImagePattern references an Image with no inline bytes");
    return UINT32_MAX;
  }
  auto asset = std::make_unique<ImageAsset>();
  asset->data = imageData;
  asset->kind = ImageAssetKind::Raster;
  // Decode image header to get actual dimensions; Codec must write non-zero
  // width/height or the Reader will degrade to 1x1 at load time.
  auto codec = tgfx::ImageCodec::MakeFrom(imageData);
  if (codec != nullptr) {
    asset->width = codec->width();
    asset->height = codec->height();
  }
  std::string semanticKey = image->filePath;  // empty OK for pure-inline images
  return RegisterImage(ctx, doc, std::move(asset), image, std::move(semanticKey));
}

void BakeColorSource(BakeContext& ctx, PAGDocument& doc, const pagx::ColorSource* src,
                     ShapeStyleData& out) {
  if (src == nullptr) {
    // Fill / Stroke with a null ColorSource stays at the SolidColor default
    // (opaque black). Downstream wire is just the 4-byte common header.
    out.sourceType = ColorSourceType::SolidColor;
    return;
  }
  switch (src->nodeType()) {
    case pagx::NodeType::SolidColor: {
      const auto& s = *static_cast<const pagx::SolidColor*>(src);
      out.sourceType = ColorSourceType::SolidColor;
      out.color = MakeProp(tgfx::Color{s.color.red, s.color.green, s.color.blue, s.color.alpha});
      break;
    }
    case pagx::NodeType::LinearGradient: {
      const auto& g = *static_cast<const pagx::LinearGradient*>(src);
      out.sourceType = ColorSourceType::LinearGradient;
      PopulateGradientStops(g, out);
      out.gradientMatrix = MakeProp(BakeMatrix(g.matrix));
      out.fitsToGeometry = g.fitsToGeometry;
      out.startPoint = MakeProp(tgfx::Point{g.startPoint.x, g.startPoint.y});
      out.endPoint = MakeProp(tgfx::Point{g.endPoint.x, g.endPoint.y});
      break;
    }
    case pagx::NodeType::RadialGradient: {
      const auto& g = *static_cast<const pagx::RadialGradient*>(src);
      out.sourceType = ColorSourceType::RadialGradient;
      PopulateGradientStops(g, out);
      out.gradientMatrix = MakeProp(BakeMatrix(g.matrix));
      out.fitsToGeometry = g.fitsToGeometry;
      out.center = MakeProp(tgfx::Point{g.center.x, g.center.y});
      out.radius = MakeProp(g.radius);
      break;
    }
    case pagx::NodeType::ConicGradient: {
      const auto& g = *static_cast<const pagx::ConicGradient*>(src);
      out.sourceType = ColorSourceType::ConicGradient;
      PopulateGradientStops(g, out);
      out.gradientMatrix = MakeProp(BakeMatrix(g.matrix));
      out.fitsToGeometry = g.fitsToGeometry;
      out.center = MakeProp(tgfx::Point{g.center.x, g.center.y});
      // ConicGradient has no `radius` of its own — PAGDocument keeps the
      // field at default 0 (Inflater reads startAngle/endAngle instead).
      out.startAngle = MakeProp(g.startAngle);
      out.endAngle = MakeProp(g.endAngle);
      break;
    }
    case pagx::NodeType::DiamondGradient: {
      const auto& g = *static_cast<const pagx::DiamondGradient*>(src);
      out.sourceType = ColorSourceType::DiamondGradient;
      PopulateGradientStops(g, out);
      out.gradientMatrix = MakeProp(BakeMatrix(g.matrix));
      out.fitsToGeometry = g.fitsToGeometry;
      out.center = MakeProp(tgfx::Point{g.center.x, g.center.y});
      out.radius = MakeProp(g.radius);
      break;
    }
    case pagx::NodeType::ImagePattern: {
      const auto& p = *static_cast<const pagx::ImagePattern*>(src);
      out.sourceType = ColorSourceType::ImagePattern;
      out.patternImageIndex = InternPatternImage(ctx, doc, p.image);
      out.tileModeX = pagx::ToTGFX(p.tileModeX);
      out.tileModeY = pagx::ToTGFX(p.tileModeY);
      out.filterMode = pagx::ToTGFX(p.filterMode);
      out.mipmapMode = pagx::ToTGFX(p.mipmapMode);
      out.scaleMode = static_cast<ScaleMode>(pagx::ToTGFX(p.scaleMode));
      out.patternMatrix = MakeProp(BakeMatrix(p.matrix));
      break;
    }
    default:
      // Unknown ColorSource subtype — leave the style at SolidColor default.
      ctx.warn(ErrorCode::InvalidEnumValue,
               "PaintBaker encountered an unknown ColorSource subtype");
      out.sourceType = ColorSourceType::SolidColor;
      break;
  }
}

std::unique_ptr<VectorElement> BakeFill(BakeContext& ctx, PAGDocument& doc, const pagx::Fill& src) {
  auto data = std::make_unique<ElementFillStyleData>();
  data->style = std::make_unique<ShapeStyleData>();
  data->style->alpha = MakeProp(src.alpha);
  // §C.6 `overrideBlendMode` marks whether the source ColorSource forced a
  // non-Normal blend. PAGX only defines `Normal` as the pass-through default
  // (there is no separate SrcOver on pagx::BlendMode — see
  // include/pagx/types/BlendMode.h), so the bit flips iff the PAGX node uses
  // anything other than Normal.
  data->style->blendMode = pagx::ToTGFX(src.blendMode);
  data->style->overrideBlendMode = (src.blendMode != pagx::BlendMode::Normal);
  BakeColorSource(ctx, doc, src.color, *data->style);
  data->fillRule = pagx::ToTGFX(src.fillRule);
  data->placement = pagx::ToTGFX(src.placement);

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::FillStyle;
  el->payload = std::move(data);
  return el;
}

std::unique_ptr<VectorElement> BakeStroke(BakeContext& ctx, PAGDocument& doc,
                                          const pagx::Stroke& src) {
  auto data = std::make_unique<ElementStrokeStyleData>();
  data->style = std::make_unique<ShapeStyleData>();
  data->style->alpha = MakeProp(src.alpha);
  data->style->blendMode = pagx::ToTGFX(src.blendMode);
  data->style->overrideBlendMode = (src.blendMode != pagx::BlendMode::Normal);
  BakeColorSource(ctx, doc, src.color, *data->style);

  data->strokeWidth = MakeProp(src.width);
  data->lineCap = pagx::ToTGFX(src.cap);
  data->lineJoin = pagx::ToTGFX(src.join);
  data->miterLimit = MakeProp(src.miterLimit);
  data->lineDashPattern = MakeProp(std::vector<float>{src.dashes.begin(), src.dashes.end()});
  data->lineDashPhase = MakeProp(src.dashOffset);
  data->lineDashAdaptive = src.dashAdaptive;
  data->strokeAlign = pagx::ToTGFX(src.align);
  data->placement = pagx::ToTGFX(src.placement);

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::StrokeStyle;
  el->payload = std::move(data);
  return el;
}

// Single-element dispatch by NodeType. Phase 6 adds Fill / Stroke; Phase 8
// adds Text / TextPath / TextModifier / TextBox routing.
std::unique_ptr<VectorElement> BakeElement(BakeContext& ctx, PAGDocument& doc,
                                           const pagx::Element* src) {
  if (src == nullptr) {
    return nullptr;
  }
  switch (src->nodeType()) {
    case pagx::NodeType::Rectangle:
      return BakeRectangle(*static_cast<const pagx::Rectangle*>(src));
    case pagx::NodeType::Ellipse:
      return BakeEllipse(*static_cast<const pagx::Ellipse*>(src));
    case pagx::NodeType::Polystar:
      return BakePolystar(*static_cast<const pagx::Polystar*>(src));
    case pagx::NodeType::Path:
      return BakePath(*static_cast<const pagx::Path*>(src));
    case pagx::NodeType::TrimPath:
      return BakeTrimPath(*static_cast<const pagx::TrimPath*>(src));
    case pagx::NodeType::RoundCorner:
      return BakeRoundCorner(*static_cast<const pagx::RoundCorner*>(src));
    case pagx::NodeType::MergePath:
      return BakeMergePath(*static_cast<const pagx::MergePath*>(src));
    case pagx::NodeType::Repeater:
      return BakeRepeater(*static_cast<const pagx::Repeater*>(src));
    case pagx::NodeType::Group:
      return BakeGroup(ctx, doc, *static_cast<const pagx::Group*>(src));
    case pagx::NodeType::TextBox:
      // TextBox inherits from Group and, after layout, behaves as a Group
      // containing positioned Text children. Phase 8 routes it through
      // BakeGroup — the nested Text elements bake themselves.
      return BakeGroup(ctx, doc, *static_cast<const pagx::Group*>(src));
    case pagx::NodeType::Fill:
      return BakeFill(ctx, doc, *static_cast<const pagx::Fill*>(src));
    case pagx::NodeType::Stroke:
      return BakeStroke(ctx, doc, *static_cast<const pagx::Stroke*>(src));
    case pagx::NodeType::Text:
      return TextBaker::BakeText(ctx, doc, *static_cast<const pagx::Text*>(src));
    case pagx::NodeType::TextPath:
      return TextBaker::BakeTextPath(ctx, *static_cast<const pagx::TextPath*>(src));
    case pagx::NodeType::TextModifier:
      return TextBaker::BakeTextModifier(ctx, *static_cast<const pagx::TextModifier*>(src));
    default:
      // Layer / resource / filter / style nodes are not valid Element
      // subtypes for `Layer::contents`; the PAGX importer should have
      // filtered them out. Defensive skip.
      return nullptr;
  }
}

}  // namespace

std::unique_ptr<VectorPayload> BakeVectorPayload(BakeContext& ctx, PAGDocument& doc,
                                                 const std::vector<pagx::Element*>& contents) {
  auto out = std::make_unique<VectorPayload>();
  for (auto* el : contents) {
    if (ctx.hasFatal()) {
      break;
    }
    auto baked = BakeElement(ctx, doc, el);
    if (baked != nullptr) {
      out->contents.push_back(std::move(baked));
    }
  }
  return out;
}

}  // namespace pagx::pag
