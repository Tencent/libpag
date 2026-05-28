/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/html/importer/HTMLLayerBuilder.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include "pagx/PAGXDocument.h"
#include "pagx/html/HTMLWriter.h"
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLDiagnosticSink.h"
#include "pagx/html/importer/HTMLIdAllocator.h"
#include "pagx/html/importer/HTMLValueParser.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/utils/StringParser.h"
#include "pagx/xml/XMLDOM.h"

namespace pagx {

using namespace pagx::html;

namespace {

// Traces the outline of a rounded rectangle with per-corner radii (TL, TR, BR, BL) inside the
// axis-aligned box (0, 0)-(W, H) onto `out`. Going clockwise starting at the top of the right
// edge — matches the canonical PAGX rendering start point for rounded rectangles, so the path
// reverses / orientation logic in shape modifiers (`ApplyRoundCorner`, `MergePath`, etc.) sees
// the same winding it would from a uniform Rectangle. Corners with radius 0 collapse to a sharp
// vertex (no cubic emitted). Caller is responsible for clamping the radii so adjacent pairs do
// not exceed the box's edge length — see the CSS scaling clamp in `HTMLStyleCascade`.
void BuildPerCornerRoundedRectPathData(PathData& out, float w, float h, float tl, float tr,
                                       float brad, float bl) {
  const float k = 1.0f - BEZIER_KAPPA;
  out.moveTo(w, tr);
  // Right edge.
  out.lineTo(w, h - brad);
  if (brad > 0) {
    out.cubicTo(w, h - brad * k, w - brad * k, h, w - brad, h);
  }
  // Bottom edge.
  out.lineTo(bl, h);
  if (bl > 0) {
    out.cubicTo(bl * k, h, 0, h - bl * k, 0, h - bl);
  }
  // Left edge.
  out.lineTo(0, tl);
  if (tl > 0) {
    out.cubicTo(0, tl * k, tl * k, 0, tl, 0);
  }
  // Top edge.
  out.lineTo(w - tr, 0);
  if (tr > 0) {
    out.cubicTo(w - tr * k, 0, w, tr * k, w, tr);
  }
  out.close();
}

}  // namespace

HTMLLayerBuilder::HTMLLayerBuilder(HTMLDiagnosticSink& sink, HTMLValueParser& valueParser,
                                   HTMLIdAllocator& idAllocator)
    : _diagnostics(sink), _valueParser(valueParser), _idAllocator(idAllocator) {
  (void)_idAllocator;  // Reserved for future per-layer id assignment paths.
}

void HTMLLayerBuilder::bindDocument(PAGXDocument* document) {
  _document = document;
}

bool HTMLLayerBuilder::hasBackgroundVisuals(const HTMLBoxAttributes& box) {
  return box.backgroundColorSet || !box.backgroundImage.empty() || box.borderRadiusSet ||
         box.borderSet || !box.boxShadow.empty() || !box.backdropFilter.empty();
}

bool HTMLLayerBuilder::requiresInnerHost(const HTMLBoxAttributes& box) {
  return box.paddingSet || box.displayFlex || box.gapSet;
}

Layer* HTMLLayerBuilder::createInnerHost(Layer* outer, const HTMLBoxAttributes& box) {
  auto inner = _document->makeNode<Layer>();
  inner->percentWidth = 100.0f;
  inner->percentHeight = 100.0f;
  applyLayoutAttributes(inner, box);
  outer->children.push_back(inner);
  return inner;
}

Fill* HTMLLayerBuilder::buildSolidFill(const Color& color) {
  auto fill = _document->makeNode<Fill>();
  auto solid = _document->makeNode<SolidColor>();
  solid->color = color;
  fill->color = solid;
  return fill;
}

ColorSource* HTMLLayerBuilder::parseGradientByValue(const std::string& value) {
  std::string trimmed = Trim(value);
  if (trimmed.empty()) return nullptr;
  std::string lower = ToLower(trimmed);
  if (lower.compare(0, 16, "linear-gradient(") == 0) {
    return _valueParser.parseLinearGradient(trimmed);
  }
  if (lower.compare(0, 16, "radial-gradient(") == 0) {
    return _valueParser.parseRadialGradient(trimmed);
  }
  if (lower.compare(0, 15, "conic-gradient(") == 0) {
    return _valueParser.parseConicGradient(trimmed);
  }
  return nullptr;
}

void HTMLLayerBuilder::applySizeAndPosition(Layer* layer, const HTMLBoxAttributes& box) {
  if (!std::isnan(box.widthPx)) layer->width = box.widthPx;
  if (!std::isnan(box.heightPx)) layer->height = box.heightPx;
  if (!std::isnan(box.widthPct)) layer->percentWidth = box.widthPct;
  if (!std::isnan(box.heightPct)) layer->percentHeight = box.heightPct;
  if (box.absolute) {
    layer->includeInLayout = false;
    if (!std::isnan(box.leftPx)) layer->left = box.leftPx;
    if (!std::isnan(box.rightPx)) layer->right = box.rightPx;
    if (!std::isnan(box.topPx)) layer->top = box.topPx;
    if (!std::isnan(box.bottomPx)) layer->bottom = box.bottomPx;
  }
  // `flex` is an "as-a-child" property; apply it on the outermost Layer regardless of
  // whether this Layer is a container, leaf, or image.
  if (box.flexGrowSet) {
    layer->flex = box.flexGrow;
  }
}

void HTMLLayerBuilder::applyLayoutAttributes(Layer* layer, const HTMLBoxAttributes& box) {
  if (box.displayFlex) {
    layer->layout = box.flexRow ? LayoutMode::Horizontal : LayoutMode::Vertical;
  }
  if (box.gapSet) layer->gap = box.gapPx;
  if (box.paddingSet) layer->padding = box.padding;
  if (!box.alignItems.empty()) {
    static const std::unordered_map<std::string, Alignment> alignTable = {
        {"stretch", Alignment::Stretch},  {"center", Alignment::Center},
        {"flex-start", Alignment::Start}, {"start", Alignment::Start},
        {"flex-end", Alignment::End},     {"end", Alignment::End},
    };
    auto it = alignTable.find(box.alignItems);
    if (it != alignTable.end()) {
      layer->alignment = it->second;
    } else {
      _diagnostics.warn("html: unsupported align-items '" + box.alignItems + "'");
    }
  }
  if (!box.justifyContent.empty()) {
    static const std::unordered_map<std::string, Arrangement> arrangementTable = {
        {"flex-start", Arrangement::Start},
        {"start", Arrangement::Start},
        {"center", Arrangement::Center},
        {"flex-end", Arrangement::End},
        {"end", Arrangement::End},
        {"space-between", Arrangement::SpaceBetween},
        {"space-evenly", Arrangement::SpaceEvenly},
        {"space-around", Arrangement::SpaceAround},
    };
    auto it = arrangementTable.find(box.justifyContent);
    if (it != arrangementTable.end()) {
      layer->arrangement = it->second;
    } else {
      _diagnostics.warn("html: unsupported justify-content '" + box.justifyContent + "'");
    }
  }
}

Element* HTMLLayerBuilder::buildBackgroundGeometry(const HTMLBoxAttributes& box) {
  // Fast path: no border-radius authored, or every corner shares the same radius. PAGX
  // `Rectangle` collapses to a single uniform `roundness`, so emit it as a `percentWidth /
  // percentHeight = 100%` shape that adapts to whatever the parent layer ends up being. This
  // keeps the common case (border-radius: 12px, border-radius: 50%, all-zeros, ...) on the
  // same compact representation older optimisations and tests rely on.
  if (!box.borderRadiusSet || box.borderRadiusUniform) {
    auto* rect = _document->makeNode<Rectangle>();
    rect->percentWidth = 100.0f;
    rect->percentHeight = 100.0f;
    rect->roundness = box.borderRadiusSet ? box.borderRadiusTLPx : 0.0f;
    return rect;
  }

  // Asymmetric per-corner radii. PAGX has no native per-corner rounded-rectangle primitive
  // (neither `Rectangle.roundness` nor `RoundCorner.radius` carries more than a single value),
  // so we synthesise a `Path` whose `PathData` traces the exact outline. Concrete coordinates
  // require the box's px size; for the rare case where the author left it unsized fall back to
  // the legacy single-roundness approximation with a diagnostic — the importer otherwise can't
  // honour the per-corner geometry without a layout pass.
  if (std::isnan(box.widthPx) || std::isnan(box.heightPx) || box.widthPx <= 0.0f ||
      box.heightPx <= 0.0f) {
    float maxR = std::max(
        {box.borderRadiusTLPx, box.borderRadiusTRPx, box.borderRadiusBRPx, box.borderRadiusBLPx});
    _diagnostics.warn(
        "html: per-corner border-radius without fixed px width/height; approximated with the "
        "largest radius (PAGX Path requires concrete dimensions)");
    auto* rect = _document->makeNode<Rectangle>();
    rect->percentWidth = 100.0f;
    rect->percentHeight = 100.0f;
    rect->roundness = maxR;
    return rect;
  }

  auto* data = _document->makeNode<PathData>();
  BuildPerCornerRoundedRectPathData(*data, box.widthPx, box.heightPx, box.borderRadiusTLPx,
                                    box.borderRadiusTRPx, box.borderRadiusBRPx,
                                    box.borderRadiusBLPx);
  auto* path = _document->makeNode<Path>();
  path->data = data;
  // Path geometry uses the parent layer's local coordinate space (already laid out at the
  // layer's `width` x `height` by `applySizeAndPosition`), so the path's own origin offset
  // stays at zero — `renderPosition` then resolves to the layer top-left and `renderScale`
  // resolves to 1 since the path bounds equal the layer bounds.
  path->position = {0.0f, 0.0f};
  return path;
}

bool HTMLLayerBuilder::applyBackgroundVisuals(Layer* layer, const HTMLBoxAttributes& box) {
  // `background-clip: text` redirects the gradient to descendant text fills (see
  // `convertTextLeaf` -> `buildTextFill`). When the element also has a gradient
  // `background-image`, suppress the rectangle + gradient Fill that would otherwise
  // paint a coloured block behind the text.
  if (box.backgroundClipText && !box.backgroundImage.empty() &&
      box.backgroundImage.find("gradient") != std::string::npos) {
    return false;
  }
  bool emitted = false;
  // `geometry` is the shape node (Rectangle or Path) that anchors the Fill / Stroke chain
  // emitted below. We only allocate it when the box actually carries a paintable visual.
  Element* geometry = nullptr;
  if (box.backgroundColorSet || !box.backgroundImage.empty() || box.borderRadiusSet ||
      box.borderSet) {
    geometry = buildBackgroundGeometry(box);
    layer->contents.push_back(geometry);
    emitted = true;
  }
  applyBackgroundFill(layer, box, geometry, emitted);
  applyBorderStroke(layer, box, geometry, emitted);
  applyBoxShadows(layer, box, emitted);
  applyBackdropFilter(layer, box, emitted);
  return emitted;
}

void HTMLLayerBuilder::applyBackgroundFill(Layer* layer, const HTMLBoxAttributes& box,
                                           Element* geometry, bool& emitted) {
  if (!geometry) return;
  if (box.backgroundColorSet && box.backgroundImage.empty()) {
    layer->contents.push_back(buildSolidFill(box.backgroundColor));
    emitted = true;
    return;
  }
  if (box.backgroundImage.empty()) return;

  // CSS allows stacking multiple gradients in `background-image` separated by top-level
  // commas, with the first listed gradient painted on top. PAGX paints Fills in the order
  // they appear in `contents` (later Fills cover earlier ones), so we emit gradients in
  // reverse so the CSS-topmost gradient becomes the last (topmost) Fill.
  std::string bg = Trim(box.backgroundImage);
  auto layers = SplitTopLevelCommas(bg);
  std::vector<ColorSource*> colors;
  colors.reserve(layers.size());
  bool anyUnsupported = false;
  for (const auto& part : layers) {
    if (auto* color = parseGradientByValue(part)) {
      colors.push_back(color);
    } else {
      anyUnsupported = true;
      std::string lower = ToLower(part);
      if (lower.compare(0, 4, "url(") == 0) {
        _diagnostics.warn("html: background-image '" + part + "' (url) not supported; use <img>");
      } else {
        _diagnostics.warn("html: background-image '" + part + "' not supported");
      }
    }
  }

  if (!colors.empty()) {
    for (auto it = colors.rbegin(); it != colors.rend(); ++it) {
      auto fill = _document->makeNode<Fill>();
      fill->color = *it;
      layer->contents.push_back(fill);
    }
    emitted = true;
    return;
  }

  // Every gradient failed to parse. Fall back to the background-color, matching the
  // previous single-gradient behaviour so text on top of the box stays legible.
  if (anyUnsupported && box.backgroundColorSet) {
    auto fill = _document->makeNode<Fill>();
    auto solid = _document->makeNode<SolidColor>();
    solid->color = box.backgroundColor;
    fill->color = solid;
    layer->contents.push_back(fill);
    emitted = true;
  }
}

void HTMLLayerBuilder::applyBorderStroke(Layer* layer, const HTMLBoxAttributes& box,
                                         Element* geometry, bool& emitted) {
  if (!geometry || !box.borderSet) return;
  auto stroke = _document->makeNode<Stroke>();
  auto solid = _document->makeNode<SolidColor>();
  solid->color = box.borderColor;
  stroke->color = solid;
  stroke->width = box.borderWidthPx;
  stroke->align = StrokeAlign::Inside;
  // CSS `border-style: dashed | dotted` maps onto Stroke's dash pattern.
  // Chromium-ish proportions: `dashed` paints rectangular dashes 2x the line
  // width separated by 1x gaps; `dotted` paints round dots with diameter
  // equal to the line width, spaced 1 width apart (a zero-length dash with
  // a Round cap renders as a circle of radius w/2, and centre-to-centre
  // spacing of 2w yields a 1w gap between dot edges).
  if (box.borderStyle == BorderStyle::Dashed) {
    const float w = box.borderWidthPx;
    stroke->dashes = {2.0f * w, w};
  } else if (box.borderStyle == BorderStyle::Dotted) {
    const float w = box.borderWidthPx;
    stroke->cap = LineCap::Round;
    stroke->dashes = {0.0f, 2.0f * w};
  }
  layer->contents.push_back(stroke);
  emitted = true;
}

void HTMLLayerBuilder::applyBoxShadows(Layer* layer, const HTMLBoxAttributes& box, bool& emitted) {
  if (box.boxShadow.empty()) return;
  // CSS spec defines the `box-shadow` blur-radius as the standard deviation of the Gaussian
  // blur, doubled. PAGX layer-style `blurX/blurY` values flow into tgfx's `ImageFilter::Blur`
  // (and SVG `feGaussianBlur stdDeviation`), both of which take Gaussian σ directly. Halving
  // the parsed blur-radius lands on the matching σ. NOTE: this conversion is specific to
  // `box-shadow` — the `filter: drop-shadow()` consumer keeps its blur as-is, since the CSS
  // Filter spec defines that function's blur-radius as σ already.
  auto shadows = _valueParser.parseShadowList(box.boxShadow);
  for (auto& s : shadows) {
    const float sigma = s.blur * 0.5f;
    if (s.inset) {
      auto inner = _document->makeNode<InnerShadowStyle>();
      inner->offsetX = s.offsetX;
      inner->offsetY = s.offsetY;
      inner->blurX = sigma;
      inner->blurY = sigma;
      inner->color = s.color;
      layer->styles.push_back(inner);
    } else {
      auto drop = _document->makeNode<DropShadowStyle>();
      drop->offsetX = s.offsetX;
      drop->offsetY = s.offsetY;
      drop->blurX = sigma;
      drop->blurY = sigma;
      drop->color = s.color;
      layer->styles.push_back(drop);
    }
  }
  if (!shadows.empty()) emitted = true;
}

void HTMLLayerBuilder::applyBackdropFilter(Layer* layer, const HTMLBoxAttributes& box,
                                           bool& emitted) {
  if (box.backdropFilter.empty()) return;
  auto steps = _valueParser.parseFilterChain(box.backdropFilter);
  for (auto& step : steps) {
    if (step.kind == HTMLValueParser::FilterStep::Kind::Blur) {
      auto bg = _document->makeNode<BackgroundBlurStyle>();
      bg->blurX = step.blurX;
      bg->blurY = step.blurY;
      layer->styles.push_back(bg);
      emitted = true;
    } else {
      _diagnostics.warn("html: backdrop-filter '" + step.raw + "' not supported");
    }
  }
}

void HTMLLayerBuilder::applyBoxTransform(Layer* layer, const HTMLBoxAttributes& box,
                                         const std::shared_ptr<DOMNode>& element) {
  if (!box.transform.valid) return;
  Matrix m = box.transform.matrix;
  if (m.isIdentity()) return;
  // Resolve the pivot (cx, cy). CSS resolves `transform-origin: 50% 50%`
  // (the default) to the box's geometric centre; non-default origins set
  // `originXPx/originYPx` during parsing so we honour the requested pivot
  // here. Without explicit width/height on a default-origin transform we
  // can't compute the centre, so the transform falls back to a top-left
  // pivot — usually a visible mismatch, hence the warning.
  float cx = NAN;
  float cy = NAN;
  if (!std::isnan(box.transform.originXPx) && !std::isnan(box.transform.originYPx)) {
    cx = box.transform.originXPx;
    cy = box.transform.originYPx;
  } else if (!std::isnan(box.widthPx) && !std::isnan(box.heightPx)) {
    cx = box.widthPx * 0.5f;
    cy = box.heightPx * 0.5f;
  } else {
    std::string tag = (element ? element->name : std::string("?"));
    _diagnostics.warn("html: transform on '<" + tag +
                      ">' without explicit width/height; transform-origin defaults to top-left "
                      "and may differ from CSS transform-origin: 50% 50%");
  }
  if (!std::isnan(cx) && !std::isnan(cy)) {
    Matrix toCenter = Matrix::Translate(cx, cy);
    Matrix fromCenter = Matrix::Translate(-cx, -cy);
    m = toCenter * m * fromCenter;
  }
  layer->matrix = m;
}

void HTMLLayerBuilder::applyLayerAttributes(Layer* layer, const std::shared_ptr<DOMNode>& element,
                                            const HTMLBoxAttributes& box) {
  if (box.opacitySet) layer->alpha = box.opacity;
  if (!box.mixBlendMode.empty()) {
    BlendMode mode = BlendModeFromString(box.mixBlendMode);
    layer->blendMode = mode;
  }
  if (box.clipOverflow) layer->clipToBounds = true;

  // filter chain (excluding backdrop-filter, which is handled as a Layer style).
  if (!box.filter.empty()) {
    auto steps = _valueParser.parseFilterChain(box.filter);
    for (auto& step : steps) {
      if (step.kind == HTMLValueParser::FilterStep::Kind::Blur) {
        auto blur = _document->makeNode<BlurFilter>();
        blur->blurX = step.blurX;
        blur->blurY = step.blurY;
        layer->filters.push_back(blur);
      } else if (step.kind == HTMLValueParser::FilterStep::Kind::DropShadow) {
        auto drop = _document->makeNode<DropShadowFilter>();
        drop->offsetX = step.shadow.offsetX;
        drop->offsetY = step.shadow.offsetY;
        drop->blurX = step.shadow.blur;
        drop->blurY = step.shadow.blur;
        drop->color = step.shadow.color;
        layer->filters.push_back(drop);
      } else {
        _diagnostics.warn("html: filter '" + step.raw + "' not supported");
      }
    }
  }

  // data-* attributes -> customData
  for (const auto& attr : element->attributes) {
    if (attr.name.compare(0, 5, "data-") == 0) {
      std::string key = attr.name.substr(5);
      if (IsValidCustomDataKey(key)) {
        layer->customData[key] = attr.value;
      } else {
        _diagnostics.warn("html: invalid data-* attribute name '" + attr.name + "'");
      }
    }
  }
  // href on <a>
  if (element->name == "a") {
    auto* href = element->findAttribute("href");
    if (href && !href->empty() && IsValidCustomDataKey("href")) {
      layer->customData["href"] = *href;
    }
  }
}

Layer* HTMLLayerBuilder::maybeSplitBoxShadowFromClip(Layer* inner) {
  if (inner == nullptr || !inner->clipToBounds || inner->styles.empty()) {
    return inner;
  }

  // Partition styles: drop shadows are hoisted onto the wrapper; inset/background blur stay
  // with the clipped inner since CSS already paints them inside the element's box.
  std::vector<LayerStyle*> hoisted;
  std::vector<LayerStyle*> kept;
  hoisted.reserve(inner->styles.size());
  kept.reserve(inner->styles.size());
  for (auto* s : inner->styles) {
    if (s != nullptr && s->nodeType() == NodeType::DropShadowStyle) {
      hoisted.push_back(s);
    } else {
      kept.push_back(s);
    }
  }
  if (hoisted.empty()) {
    return inner;
  }

  inner->styles = std::move(kept);

  auto* outer = _document->makeNode<Layer>();

  // Move the layout slot (the wrapper occupies the same space in its parent that `inner`
  // previously held).
  outer->width = inner->width;
  outer->height = inner->height;
  outer->percentWidth = inner->percentWidth;
  outer->percentHeight = inner->percentHeight;
  outer->left = inner->left;
  outer->right = inner->right;
  outer->top = inner->top;
  outer->bottom = inner->bottom;
  outer->centerX = inner->centerX;
  outer->centerY = inner->centerY;
  outer->includeInLayout = inner->includeInLayout;
  outer->flex = inner->flex;

  // Move "through-effects" that semantically wrap the element + its shadow in CSS.
  outer->alpha = inner->alpha;
  outer->blendMode = inner->blendMode;
  outer->matrix = inner->matrix;
  outer->matrix3D = inner->matrix3D;
  outer->preserve3D = inner->preserve3D;
  outer->filters = std::move(inner->filters);
  outer->customData = std::move(inner->customData);
  outer->id = std::move(inner->id);
  outer->styles = std::move(hoisted);

  // Reset the moved fields on `inner` so the wrapping is invisible to downstream
  // consumers. `inner` now fills the wrapper exactly; its clipToBounds / contents
  // / styles-that-stayed remain in place.
  inner->width = NAN;
  inner->height = NAN;
  inner->percentWidth = 100.0f;
  inner->percentHeight = 100.0f;
  inner->left = NAN;
  inner->right = NAN;
  inner->top = NAN;
  inner->bottom = NAN;
  inner->centerX = NAN;
  inner->centerY = NAN;
  inner->includeInLayout = true;
  inner->flex = 0.0f;
  inner->alpha = 1.0f;
  inner->blendMode = BlendMode::Normal;
  inner->matrix = Matrix{};
  inner->matrix3D = Matrix3D{};
  inner->preserve3D = false;

  outer->children.push_back(inner);
  return outer;
}

Layer* HTMLLayerBuilder::wrapWithMargin(Layer* inner, const HTMLBoxAttributes& box) {
  if (inner == nullptr) return inner;
  if (box.marginTopPx == 0.0f && box.marginRightPx == 0.0f && box.marginBottomPx == 0.0f &&
      box.marginLeftPx == 0.0f) {
    return inner;
  }

  // Absolute children: CSS positions the box `margin-<side>` further from the containing
  // block's padding edge than the matching offset the author already wrote, so folding
  // margin straight into the existing edge anchors reproduces the visual position without
  // an extra Layer. Anchors that the author left unset stay unset — CSS would not have
  // moved that side either.
  if (box.absolute) {
    if (!std::isnan(inner->left)) inner->left += box.marginLeftPx;
    if (!std::isnan(inner->right)) inner->right += box.marginRightPx;
    if (!std::isnan(inner->top)) inner->top += box.marginTopPx;
    if (!std::isnan(inner->bottom)) inner->bottom += box.marginBottomPx;
    return inner;
  }

  // Flow / flex children: build a transparent outer Layer whose `padding` reproduces the
  // four-side margin, and hand the parent-facing layout slot (flex, percent size, explicit
  // edge anchors, includeInLayout) over to it. The original `inner` keeps its visuals and
  // its own width/height; padding then nests `inner` inside the wrapper's padded box,
  // which is the same offset CSS margin produces relative to the parent's flow / flex
  // measurement (`outer size = inner size + margin` on every side). When `inner` carries
  // no positional constraints, `LayoutNode::PerformConstraintLayout` already routes its
  // resolved position through the wrapper's `padding.left / padding.top`, so we don't
  // need to write explicit `inner->left / inner->top` here — the default (NaN, NaN) origin
  // lands on the padded-box top-left automatically.
  auto* outer = _document->makeNode<Layer>();
  outer->padding.top = box.marginTopPx;
  outer->padding.right = box.marginRightPx;
  outer->padding.bottom = box.marginBottomPx;
  outer->padding.left = box.marginLeftPx;

  outer->left = inner->left;
  outer->right = inner->right;
  outer->top = inner->top;
  outer->bottom = inner->bottom;
  outer->centerX = inner->centerX;
  outer->centerY = inner->centerY;
  outer->percentWidth = inner->percentWidth;
  outer->percentHeight = inner->percentHeight;
  outer->includeInLayout = inner->includeInLayout;
  outer->flex = inner->flex;

  inner->left = NAN;
  inner->right = NAN;
  inner->top = NAN;
  inner->bottom = NAN;
  inner->centerX = NAN;
  inner->centerY = NAN;
  inner->percentWidth = NAN;
  inner->percentHeight = NAN;
  inner->includeInLayout = true;
  inner->flex = 0.0f;

  outer->children.push_back(inner);
  return outer;
}

void HTMLLayerBuilder::emitTextDecorationLine(Layer* host, const Color& /*textColor*/,
                                              const Color& decorationColor,
                                              bool decorationColorDiffers, float bottom,
                                              float centerY) {
  auto rect = _document->makeNode<Rectangle>();
  rect->percentWidth = 100.0f;
  rect->height = 1.0f;
  if (!std::isnan(bottom)) rect->bottom = bottom;
  if (!std::isnan(centerY)) rect->centerY = centerY;
  if (decorationColorDiffers) {
    auto group = _document->makeNode<Group>();
    group->elements.push_back(rect);
    group->elements.push_back(buildSolidFill(decorationColor));
    host->contents.push_back(group);
  } else {
    host->contents.push_back(rect);
    host->contents.push_back(buildSolidFill(decorationColor));
  }
}

}  // namespace pagx
