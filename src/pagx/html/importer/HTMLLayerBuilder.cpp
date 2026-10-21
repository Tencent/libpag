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
#include "pagx/html/importer/HTMLSvgFilterDecoder.h"
#include "pagx/html/importer/HTMLValueParser.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorMatrixFilter.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Gradient.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/svg/SVGBlendMode.h"
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

void BuildShadowBoxPath(PathData& out, float width, float height, const float radii[4],
                        bool ellipse) {
  if (ellipse) {
    const float radius = width * 0.5f;
    BuildPerCornerRoundedRectPathData(out, width, width, radius, radius, radius, radius);
    out.transform(Matrix::Scale(1, height / width));
  } else {
    BuildPerCornerRoundedRectPathData(out, width, height, radii[0], radii[1], radii[2], radii[3]);
  }
}

// Hands the parent-facing layout slot (positional anchors, percent sizing, and flex
// participation) over from `inner` to `outer`. Used by the wrapper helpers that hoist a Layer
// "out from under" inner so the wrapper occupies the same space `inner` previously held.
void MoveLayoutSlot(Layer* outer, Layer* inner) {
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
}

// Resets the positional anchors and flex slot on `inner` so the parent layout will route
// through the wrapper instead. `percentWidth`/`percentHeight` are NOT reset here because the
// two wrapper paths disagree: `maybeSplitBoxShadowFromClip` wants `inner` to fill the wrapper
// (100%) while `wrapForMargin` wants `inner` to keep its own intrinsic size (NaN).
void ResetLayoutAnchors(Layer* inner) {
  inner->left = NAN;
  inner->right = NAN;
  inner->top = NAN;
  inner->bottom = NAN;
  inner->centerX = NAN;
  inner->centerY = NAN;
  inner->includeInLayout = true;
  inner->flex = 0.0f;
}

// `background-clip: text` paints the glyphs with the element's background, so the rectangle that
// would otherwise paint behind the text must not be emitted — the paint is consumed by the
// descendant text fill instead (a gradient through `textFillImage`, a solid colour through
// `textFillSolid`). Without a gradient layer or a solid `background-color` there is nothing to
// redirect and the element keeps its normal box background. Keep this predicate separate from
// the visual-emission code because replaced elements may still have a foreground fill, border,
// shadow, or backdrop filter that must survive the redirect.
bool ClipsBackgroundToText(const HTMLBoxAttributes& box) {
  if (!box.backgroundClipText) return false;
  if (ToLower(box.backgroundImage).find("gradient(") != std::string::npos) return true;
  return box.backgroundImage.empty() && box.backgroundColorSet && box.backgroundColor.alpha > 0.0f;
}

}  // namespace

BlendMode HTMLLayerBuilder::resolveBackgroundBlendMode(const std::string& value) {
  if (value.empty()) return BlendMode::Normal;
  // `background-blend-mode` may list one keyword per background layer. We apply a single blend
  // to every gradient/image Fill, so key off the first top-level token (the common single-value
  // case is unaffected). `SVGBlendModeFromString` maps the CSS keyword directly and returns
  // Normal for `normal` / any unrecognised value.
  auto tokens = SplitTopLevelCommas(value);
  std::string first = tokens.empty() ? value : Trim(tokens.front());
  return SVGBlendModeFromString(first);
}

HTMLLayerBuilder::HTMLLayerBuilder(HTMLDiagnosticSink& sink, HTMLValueParser& valueParser,
                                   HTMLInlineSvgEmitter& svgEmitter, HTMLIdAllocator& idAllocator)
    : _diagnostics(sink), _valueParser(valueParser), _idAllocator(idAllocator),
      _filterDecoder(sink, svgEmitter, valueParser) {
}

void HTMLLayerBuilder::bindDocument(PAGXDocument* document) {
  _document = document;
  _outerBoxShadows.clear();
  _filterDecoder.bindDocument(document);
}

bool HTMLLayerBuilder::hasBackgroundVisuals(const HTMLBoxAttributes& box) {
  bool hasBoxBackground =
      !ClipsBackgroundToText(box) && (box.backgroundColorSet || !box.backgroundImage.empty());
  return hasBoxBackground || box.borderRadiusSet || box.borderSet || !box.boxShadow.empty() ||
         !box.backdropFilter.empty();
}

bool HTMLLayerBuilder::hasLayoutHostAttributes(const HTMLBoxAttributes& box) {
  return box.paddingSet || box.displayFlex || box.gapSet;
}

bool HTMLLayerBuilder::requiresInnerHost(const HTMLBoxAttributes& box) {
  if (!box.padding.isZero()) return true;
  // Flex-flow children sit inside the CSS border edge. The importer models that edge as extra
  // padding on the layout host, so a bordered flex box still needs paint/layout isolation even
  // when its authored padding is zero.
  return box.displayFlex && box.borderSet && box.borderWidthPx > 0.0f;
}

Layer* HTMLLayerBuilder::createInnerHost(Layer* outer, const HTMLBoxAttributes& box) {
  auto inner = _document->makeNode<Layer>();
  inner->percentWidth = 100.0f;
  inner->percentHeight = 100.0f;
  applyLayoutAttributes(inner, box);
  if (box.borderSet && box.borderWidthPx > 0.0f && inner->layout != LayoutMode::None) {
    inner->padding.top += box.borderWidthPx;
    inner->padding.right += box.borderWidthPx;
    inner->padding.bottom += box.borderWidthPx;
    inner->padding.left += box.borderWidthPx;
  }
  outer->children.push_back(inner);
  return inner;
}

void HTMLLayerBuilder::appendCSSChild(Layer* host, Layer* child, const HTMLBoxAttributes& box) {
  if (!child->includeInLayout) {
    const float border = box.borderSet ? box.borderWidthPx : 0.0f;
    const auto& padding = host->padding;
    if (padding.left != border || padding.right != border || padding.top != border ||
        padding.bottom != border) {
      auto frame = _document->makeNode<Layer>();
      frame->includeInLayout = false;
      frame->left = border - padding.left;
      frame->right = border - padding.right;
      frame->top = border - padding.top;
      frame->bottom = border - padding.bottom;
      frame->children.push_back(child);
      host->children.push_back(frame);
      return;
    }
  }
  host->children.push_back(child);
}

Fill* HTMLLayerBuilder::buildSolidFill(const Color& color) {
  auto fill = _document->makeNode<Fill>();
  auto solid = _document->makeNode<SolidColor>();
  solid->color = color;
  fill->color = solid;
  return fill;
}

ColorSource* HTMLLayerBuilder::parseGradientByValue(const std::string& value, float boxWidth,
                                                    float boxHeight) {
  std::string trimmed = Trim(value);
  if (trimmed.empty()) return nullptr;
  std::string lower = ToLower(trimmed);
  // CSS `repeating-*-gradient` shares the base syntax; the parsers tile the authored period into
  // explicit stops (PAGX gradients have no native spread/repeat mode). `ExtractParenArgs` is
  // prefix-agnostic, so the same value string is handed straight through with a repeating flag.
  if (lower.compare(0, 26, "repeating-linear-gradient(") == 0) {
    // Prefer a tiled ImagePattern (crisp at any density); fall back to gradient-stop tiling when
    // the pattern is unavailable (unknown box, oblique angle, degenerate period).
    if (auto* pattern =
            _valueParser.parseRepeatingLinearGradientPattern(trimmed, boxWidth, boxHeight)) {
      return pattern;
    }
    return _valueParser.parseLinearGradient(trimmed, boxWidth, boxHeight, /*repeating=*/true);
  }
  if (lower.compare(0, 26, "repeating-radial-gradient(") == 0) {
    return _valueParser.parseRadialGradient(trimmed, boxWidth, boxHeight, /*repeating=*/true);
  }
  if (lower.compare(0, 25, "repeating-conic-gradient(") == 0) {
    return _valueParser.parseConicGradient(trimmed, /*repeating=*/true, boxWidth, boxHeight);
  }
  if (lower.compare(0, 16, "linear-gradient(") == 0) {
    return _valueParser.parseLinearGradient(trimmed, boxWidth, boxHeight);
  }
  if (lower.compare(0, 16, "radial-gradient(") == 0) {
    return _valueParser.parseRadialGradient(trimmed, boxWidth, boxHeight);
  }
  if (lower.compare(0, 15, "conic-gradient(") == 0) {
    return _valueParser.parseConicGradient(trimmed, false, boxWidth, boxHeight);
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
  // `border-radius: 50%` (and the equivalent `50% / 50%`) inscribes a true ellipse in the box.
  // PAGX `Rectangle` only carries a single scalar roundness, so on a non-square box it would
  // render a pill (semicircular ends + a straight middle) instead of the ellipse CSS intends.
  // Emit a PAGX `Ellipse` sized to 100% of the layer so it adapts to the laid-out box, exactly
  // like the Rectangle fast path below.
  if (box.borderRadiusEllipse) {
    auto* ellipse = _document->makeNode<Ellipse>();
    ellipse->percentWidth = 100.0f;
    ellipse->percentHeight = 100.0f;
    return ellipse;
  }

  // Fast path: no border-radius authored, or every corner shares the same radius. PAGX
  // `Rectangle` collapses to a single uniform `roundness`, so emit it as a `percentWidth /
  // percentHeight = 100%` shape that adapts to whatever the parent layer ends up being. This
  // keeps the common case (border-radius: 12px, all-zeros, ...) on the same compact
  // representation older optimisations and tests rely on.
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

bool HTMLLayerBuilder::applyBackgroundVisuals(Layer* layer, const HTMLBoxAttributes& box,
                                              Fill* foregroundFill) {
  if (!ClipsBackgroundToText(box) && usesBackgroundLayout(box)) {
    bool emitted = applyBackgroundLayers(layer, box);
    applyBoxShadows(layer, box, emitted);
    // A Layer's contents paint below all of its children. Put replaced content and border
    // together after the background stack so tiled backgrounds cannot cover the <img>.
    if (foregroundFill || box.borderSet) {
      auto* foreground = _document->makeNode<Layer>();
      foreground->includeInLayout = false;
      foreground->width = box.widthPx;
      foreground->height = box.heightPx;
      auto* geometry = buildBackgroundGeometry(box);
      foreground->contents.push_back(geometry);
      if (foregroundFill) foreground->contents.push_back(foregroundFill);
      applyBorderStroke(foreground, box, geometry, emitted);
      layer->children.push_back(foreground);
      emitted = true;
    }
    applyBackdropFilter(layer, box, emitted);
    return emitted;
  }
  // `background-clip: text` redirects the background to descendant text fills (see
  // `convertTextLeaf` -> `buildTextFill`, and `HTMLStyleCascade::resolveInheritedStyle` for the
  // gradient / solid channels). Suppress only that box background. Replaced-element foregrounds
  // and independent box visuals (border, shadow, backdrop filter) must still be emitted.
  bool clipsBackgroundToText = ClipsBackgroundToText(box);
  bool emitted = false;
  // `geometry` is the shape node (Rectangle or Path) that anchors the Fill / Stroke chain
  // emitted below. We only allocate it when the box actually carries a paintable visual.
  Element* geometry = nullptr;
  bool hasBoxBackground =
      !clipsBackgroundToText && (box.backgroundColorSet || !box.backgroundImage.empty());
  if (foregroundFill != nullptr || hasBoxBackground || box.borderRadiusSet || box.borderSet) {
    geometry = buildBackgroundGeometry(box);
    layer->contents.push_back(geometry);
    emitted = true;
  }
  if (!clipsBackgroundToText) {
    applyBackgroundFill(layer, box, geometry, emitted);
  }
  if (foregroundFill != nullptr) {
    layer->contents.push_back(foregroundFill);
    emitted = true;
  }
  applyBorderStroke(layer, box, geometry, emitted);
  applyBoxShadows(layer, box, emitted);
  applyBackdropFilter(layer, box, emitted);
  return emitted;
}

void HTMLLayerBuilder::setBackgroundImageResolver(
    std::function<HTMLBackgroundImage(const std::string&)> resolver) {
  _backgroundImageResolver = std::move(resolver);
}

bool HTMLLayerBuilder::usesBackgroundLayout(const HTMLBoxAttributes& box) {
  if (!std::isfinite(box.widthPx) || !std::isfinite(box.heightPx)) return false;
  if (!box.backgroundClip.empty()) return true;
  if (box.backgroundImage.empty()) return false;
  return ToLower(box.backgroundImage).find("url(") != std::string::npos ||
         (!box.backgroundSize.empty() && box.backgroundSize != "auto") ||
         (!box.backgroundPosition.empty() && box.backgroundPosition != "0% 0%") ||
         (!box.backgroundOrigin.empty() && box.backgroundOrigin != "padding-box") ||
         box.borderWidthPx > 0 ||
         (!box.backgroundBlendMode.empty() && box.backgroundBlendMode != "normal");
}

Layer* HTMLLayerBuilder::makeBackgroundClip(const HTMLBoxAttributes& box,
                                            const HTMLBackgroundRect& rect) {
  auto* host = _document->makeNode<Layer>();
  host->width = box.widthPx;
  host->height = box.heightPx;
  host->includeInLayout = false;
  auto* mask = _document->makeNode<Layer>();
  mask->id = _idAllocator.generateUnique("backgroundClip");
  mask->includeInLayout = false;
  mask->width = rect.width;
  mask->height = rect.height;
  mask->left = rect.x;
  mask->top = rect.y;
  // CSS derives each inner corner by subtracting the corresponding two edge insets.
  float rx[4] = {box.borderRadiusTLPx, box.borderRadiusTRPx, box.borderRadiusBRPx,
                 box.borderRadiusBLPx};
  float ry[4] = {rx[0], rx[1], rx[2], rx[3]};
  if (box.borderRadiusEllipse) {
    for (int i = 0; i < 4; ++i) {
      rx[i] = box.widthPx * .5f;
      ry[i] = box.heightPx * .5f;
    }
  }
  float right = box.widthPx - rect.x - rect.width;
  float bottom = box.heightPx - rect.y - rect.height;
  float dx[4] = {rect.x, right, right, rect.x};
  float dy[4] = {rect.y, rect.y, bottom, bottom};
  for (int i = 0; i < 4; ++i) {
    rx[i] = std::max(0.0f, rx[i] - dx[i]);
    ry[i] = std::max(0.0f, ry[i] - dy[i]);
  }
  float scale = 1;
  for (auto pair :
       {std::make_pair(rect.width, rx[0] + rx[1]), std::make_pair(rect.width, rx[2] + rx[3]),
        std::make_pair(rect.height, ry[0] + ry[3]), std::make_pair(rect.height, ry[1] + ry[2])}) {
    if (pair.second > 0) scale = std::min(scale, pair.first / pair.second);
  }
  for (int i = 0; i < 4; ++i) {
    rx[i] *= scale;
    ry[i] *= scale;
  }
  auto* data = _document->makeNode<PathData>();
  float w = rect.width, h = rect.height, k = 1 - BEZIER_KAPPA;
  data->moveTo(rx[0], 0);
  data->lineTo(w - rx[1], 0);
  data->cubicTo(w - rx[1] * k, 0, w, ry[1] * k, w, ry[1]);
  data->lineTo(w, h - ry[2]);
  data->cubicTo(w, h - ry[2] * k, w - rx[2] * k, h, w - rx[2], h);
  data->lineTo(rx[3], h);
  data->cubicTo(rx[3] * k, h, 0, h - ry[3] * k, 0, h - ry[3]);
  data->lineTo(0, ry[0]);
  data->cubicTo(0, ry[0] * k, rx[0] * k, 0, rx[0], 0);
  data->close();
  auto* path = _document->makeNode<Path>();
  path->data = data;
  path->position = {0, 0};
  mask->contents.push_back(path);
  mask->contents.push_back(buildSolidFill(Color{1, 1, 1, 1}));
  host->mask = mask;
  host->maskType = MaskType::Contour;
  host->children.push_back(mask);
  return host;
}

bool HTMLLayerBuilder::applyBackgroundLayers(Layer* layer, const HTMLBoxAttributes& box) {
  if (box.widthPx <= 0 || box.heightPx <= 0) return false;
  auto images = SplitTopLevelCommas(box.backgroundImage);
  auto* stack = _document->makeNode<Layer>();
  stack->width = box.widthPx;
  stack->height = box.heightPx;
  stack->includeInLayout = false;
  // Background blend modes see only this element's background, not its ancestors or content.
  stack->passThroughBackground = false;
  layer->children.push_back(stack);
  if (box.backgroundColorSet) {
    auto rect = BackgroundBoxRect(
        box, BackgroundListEntry(box.backgroundClip, images.empty() ? 0 : images.size() - 1,
                                 "border-box"));
    auto* color = makeBackgroundClip(box, rect);
    auto* geometry = _document->makeNode<Rectangle>();
    geometry->width = box.widthPx;
    geometry->height = box.heightPx;
    geometry->left = geometry->top = 0;
    color->contents.push_back(geometry);
    color->contents.push_back(buildSolidFill(box.backgroundColor));
    stack->children.push_back(color);
  }
  for (size_t i = images.size(); i-- > 0;) {
    auto value = Trim(images[i]);
    if (ToLower(value) == "none") continue;
    HTMLBackgroundImage image;
    if (ToLower(value).rfind("url(", 0) == 0 && _backgroundImageResolver) {
      image = _backgroundImageResolver(value);
    }
    HTMLBackgroundLayout layout;
    if (!ResolveBackgroundLayout(box, i, image.intrinsicWidth, image.intrinsicHeight, _valueParser,
                                 layout)) {
      _diagnostics.warn("html: unsupported background layout or repeat budget exceeded at layer " +
                        std::to_string(i) + "; omitted");
      continue;
    }
    if (layout.x.count == 0 || layout.y.count == 0 || layout.clip.width <= 0 ||
        layout.clip.height <= 0)
      continue;
    if (!image.color && !image.vectorTile) {
      image.color = parseGradientByValue(value, layout.x.size, layout.y.size);
    }
    if (!image.color && !image.vectorTile) {
      _diagnostics.warn("html: unsupported background image at layer " + std::to_string(i));
      continue;
    }
    // Repeater is compact on disk but still costs geometry/render work per visible copy.
    if (static_cast<int64_t>(layout.x.count) * layout.y.count > 65536 ||
        (image.vectorTile && static_cast<int64_t>(layout.x.count) * layout.y.count > 4096)) {
      _diagnostics.warn("html: background tile budget exceeded at layer " + std::to_string(i) +
                        "; omitted");
      continue;
    }
    auto* clip = makeBackgroundClip(box, layout.clip);
    clip->blendMode =
        resolveBackgroundBlendMode(BackgroundListEntry(box.backgroundBlendMode, i, "normal"));
    stack->children.push_back(clip);
    if (image.vectorTile) {
      for (int y = 0; y < layout.y.count; ++y) {
        for (int x = 0; x < layout.x.count; ++x) {
          auto* tile = image.vectorTile(layout.x.size, layout.y.size);
          tile->left = layout.x.first + x * layout.x.step;
          tile->top = layout.y.first + y * layout.y.step;
          clip->children.push_back(tile);
        }
      }
      continue;
    }
    // Internal shared tile boundaries must not be individually antialiased: two half-covered
    // rectangles composited source-over leak the backdrop. Clip each contiguous run once,
    // retaining AA on real outer edges and on gaps introduced by `space`/`no-repeat`.
    int columns = layout.x.contiguous ? 1 : layout.x.count;
    int rows = layout.y.contiguous ? 1 : layout.y.count;
    for (int y = 0; y < rows; ++y) {
      for (int x = 0; x < columns; ++x) {
        float left = layout.x.first + x * layout.x.step;
        float top = layout.y.first + y * layout.y.step;
        auto* run = _document->makeNode<Layer>();
        run->width = layout.x.size * (layout.x.contiguous ? layout.x.count : 1);
        run->height = layout.y.size * (layout.y.contiguous ? layout.y.count : 1);
        run->left = left;
        run->top = top;
        run->includeInLayout = false;
        run->clipToBounds = true;
        clip->children.push_back(run);
        auto* paint = _document->makeNode<Layer>();
        paint->width = run->width;
        paint->height = run->height;
        paint->includeInLayout = false;
        paint->antiAlias = false;
        run->children.push_back(paint);
        auto* group = _document->makeNode<Group>();
        group->width = paint->width;
        group->height = paint->height;
        auto* tile = _document->makeNode<Rectangle>();
        tile->width = layout.x.size;
        tile->height = layout.y.size;
        tile->left = tile->top = 0;
        group->elements.push_back(tile);
        auto* fill = _document->makeNode<Fill>();
        fill->color = image.color;
        group->elements.push_back(fill);
        auto repeat = [&](int count, float dx, float dy) {
          if (count <= 1) return;
          auto* repeater = _document->makeNode<Repeater>();
          repeater->copies = static_cast<float>(count);
          repeater->position = {dx, dy};
          group->elements.push_back(repeater);
        };
        repeat(layout.x.contiguous ? layout.x.count : 1, layout.x.step, 0);
        repeat(layout.y.contiguous ? layout.y.count : 1, 0, layout.y.step);
        paint->contents.push_back(group);
      }
    }
  }
  return true;
}

namespace {

// Whether a background paint can let the layers below it show through, so the CSS
// background-color must be preserved underneath. Gradients are transparent when any stop carries
// alpha < 1; image patterns (e.g. a `repeating-linear-gradient` tile with transparent gaps) are
// treated as potentially transparent.
bool ColorSourceMayShowThrough(const ColorSource* source) {
  if (source == nullptr) return false;
  switch (source->nodeType()) {
    case NodeType::ImagePattern:
      return true;
    case NodeType::LinearGradient:
    case NodeType::RadialGradient:
    case NodeType::ConicGradient:
    case NodeType::DiamondGradient: {
      const auto* gradient = static_cast<const Gradient*>(source);
      for (const auto* stop : gradient->colorStops) {
        if (stop != nullptr && stop->color.alpha < 1.0f) return true;
      }
      return false;
    }
    default:
      return false;
  }
}

}  // namespace

void HTMLLayerBuilder::applyBackgroundFill(Layer* layer, const HTMLBoxAttributes& box,
                                           Element* geometry, bool& emitted) {
  if (!geometry) return;

  // A `url(...)` background is recovered as an `ImagePattern` fill by `HTMLParserContext`
  // (it owns the image-resource registry and native-size decoding) and pushed onto the layer
  // right after this call, so it paints on top. CSS layers the image over the
  // background-color, and decorative url() backgrounds are frequently partly transparent
  // (e.g. a low-opacity noise overlay), so the colour must still be emitted underneath here
  // rather than dropped.
  bool urlImage = !box.backgroundImage.empty() &&
                  ToLower(box.backgroundImage).find("url(") != std::string::npos;
  if (box.backgroundImage.empty() || urlImage) {
    if (box.backgroundColorSet) {
      layer->contents.push_back(buildSolidFill(box.backgroundColor));
      emitted = true;
    }
    return;
  }

  // CSS allows stacking multiple gradients in `background-image` separated by top-level
  // commas, with the first listed gradient painted on top. PAGX paints Fills in the order
  // they appear in `contents` (later Fills cover earlier ones), so we emit gradients in
  // reverse so the CSS-topmost gradient becomes the last (topmost) Fill.
  std::string bg = Trim(box.backgroundImage);
  auto layers = SplitTopLevelCommas(bg);
  std::vector<ColorSource*> colors;
  // CSS layer index of each parsed gradient, kept alongside `colors` so the per-layer
  // `background-clip` list below can be indexed by the authored layer position even when a sibling
  // layer did not parse into a gradient.
  std::vector<size_t> colorLayerIndices;
  colors.reserve(layers.size());
  colorLayerIndices.reserve(layers.size());
  bool anyUnsupported = false;
  for (size_t i = 0; i < layers.size(); i++) {
    const std::string& part = layers[i];
    if (ToLower(Trim(part)) == "none") continue;
    if (auto* color = parseGradientByValue(part, box.widthPx, box.heightPx)) {
      colors.push_back(color);
      colorLayerIndices.push_back(i);
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
    // `background-clip` carries per-layer box keywords in CSS layer order (the subset
    // transformer only keeps lists with at least one non-`border-box` layer). CSS cycles a
    // shorter clip list across the image layers, so index with modulo.
    auto clips = SplitTopLevelCommas(ToLower(Trim(box.backgroundClip)));
    for (auto& clip : clips) {
      clip = Trim(clip);
    }
    // The inset path paints each `padding-box` / `content-box` layer on its own child layer,
    // which renders above the host's `contents` (where the `border-box` layers live) but
    // below every content child. That ordering can only express the canonical CSS pattern —
    // every non-border-box layer sits above every border-box layer (a border-box layer above
    // a tighter layer would fully cover it anyway unless transparent). Any other order falls
    // back to plain border-box painting with a diagnostic.
    bool useInsetLayers = !clips.empty();
    if (useInsetLayers) {
      bool sawBorderBox = false;
      for (size_t i = 0; i < colors.size(); i++) {
        const std::string& clip = clips[colorLayerIndices[i] % clips.size()];
        if (clip == "border-box") {
          sawBorderBox = true;
        } else if (sawBorderBox) {
          useInsetLayers = false;
          _diagnostics.warn(
              "html: background-clip border-box layer above a tighter layer; painting every "
              "layer to the border box");
          break;
        }
      }
    }
    // The inset layer needs concrete px geometry: an unsized box (percent layout) or a
    // per-corner / ellipse radius has no exact inset rectangle to build.
    if (useInsetLayers && (std::isnan(box.widthPx) || std::isnan(box.heightPx) ||
                           box.widthPx <= 0.0f || box.heightPx <= 0.0f)) {
      _diagnostics.warn(
          "html: background-clip padding-box without fixed px width/height; clipping to "
          "border-box");
      useInsetLayers = false;
    }
    if (useInsetLayers && box.borderRadiusSet &&
        (!box.borderRadiusUniform || box.borderRadiusEllipse)) {
      _diagnostics.warn(
          "html: background-clip padding-box with non-uniform or ellipse border-radius; "
          "clipping to border-box");
      useInsetLayers = false;
    }
    if (!useInsetLayers) {
      clips.clear();
    }

    // `background-blend-mode` blends each background layer against the layers *below* it, with
    // the background-color as the bottom-most layer. Emit the solid colour first so the blended
    // gradient Fill has a backdrop to composite against; without a blend mode an opaque gradient
    // fully hides the colour, so the no-blend path drops it to avoid an inert extra Fill — but a
    // transparent gradient / repeating-pattern tile lets the colour show through, so it must stay.
    BlendMode blend = resolveBackgroundBlendMode(box.backgroundBlendMode);
    bool anyShowsThrough = false;
    for (auto* color : colors) {
      if (ColorSourceMayShowThrough(color)) {
        anyShowsThrough = true;
        break;
      }
    }
    // Tracks whether a background layer has already been painted beneath the current Fill. The
    // bottom-most layer has nothing below it in the element's own background, so it must draw
    // with Normal (matching CSS, where a lone/bottom layer's blend mode is a no-op) rather than
    // blending against whatever sits behind the whole layer.
    bool hasBackdrop = false;
    if ((blend != BlendMode::Normal || anyShowsThrough) && box.backgroundColorSet) {
      layer->contents.push_back(buildSolidFill(box.backgroundColor));
      hasBackdrop = true;
    }
    // Gradients are pushed in reverse CSS order (the CSS-last layer first), so the first Fill
    // emitted here is the bottom-most background layer. Border-box layers stay on the host's
    // contents (bottom of the paint stack); each tighter clip becomes an inset child layer,
    // pushed bottom-most-first so the CSS-topmost layer ends up as the topmost child.
    for (auto it = colors.rbegin(); it != colors.rend(); ++it) {
      const size_t colorIndex = static_cast<size_t>(std::distance(it, colors.rend())) - 1;
      const std::string& clip =
          clips.empty() ? std::string() : clips[colorLayerIndices[colorIndex] % clips.size()];
      if (clip == "padding-box" || clip == "content-box") {
        Padding inset = {};
        inset.top = inset.right = inset.bottom = inset.left = box.borderWidthPx;
        if (clip == "content-box") {
          inset.top += box.padding.top;
          inset.right += box.padding.right;
          inset.bottom += box.padding.bottom;
          inset.left += box.padding.left;
        }
        emitInsetBackgroundLayer(layer, box, *it, inset, hasBackdrop ? blend : BlendMode::Normal);
      } else {
        auto fill = _document->makeNode<Fill>();
        fill->color = *it;
        fill->blendMode = hasBackdrop ? blend : BlendMode::Normal;
        layer->contents.push_back(fill);
      }
      hasBackdrop = true;
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

void HTMLLayerBuilder::emitInsetBackgroundLayer(Layer* layer, const HTMLBoxAttributes& box,
                                                ColorSource* color, const Padding& inset,
                                                BlendMode blendMode) {
  auto inner = _document->makeNode<Layer>();
  inner->includeInLayout = false;
  // Constraints (`left` / `top`) rather than `x` / `y`: an out-of-flow child is still constraint
  // laid out against its parent's padding box, so `x` / `y` would be dropped whenever the host
  // carries padding. The vector-background host in `HTMLParserContext` positions itself the same
  // way.
  inner->left = inset.left;
  inner->top = inset.top;
  inner->width = std::max(0.0f, box.widthPx - inset.left - inset.right);
  inner->height = std::max(0.0f, box.heightPx - inset.top - inset.bottom);
  auto* rect = _document->makeNode<Rectangle>();
  rect->percentWidth = 100.0f;
  rect->percentHeight = 100.0f;
  // CSS shrinks the padding-box corner radius by the border width (the inner curve of a
  // rounded border). With a uniform authored radius a single scalar is exact; the max inset
  // approximates per-side insets on a uniform-radius box.
  rect->roundness = std::max(
      0.0f, box.borderRadiusTLPx - std::max({inset.top, inset.right, inset.bottom, inset.left}));
  inner->contents.push_back(rect);
  auto* fill = _document->makeNode<Fill>();
  fill->color = color;
  fill->blendMode = blendMode;
  inner->contents.push_back(fill);
  // Child layers render above the host's own contents (the border-box layers) and below the
  // content children pushed later, so the inset layer reveals the border-box layer beneath
  // as a gradient frame exactly like CSS padding-box clipping.
  layer->children.push_back(inner);
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
  bool hasSpread = false;
  for (const auto& shadow : shadows) hasSpread |= shadow.spread != 0;
  if (hasSpread && std::isfinite(box.widthPx) && std::isfinite(box.heightPx)) {
    // A spread applies to the box silhouette, independently of its background
    // alpha and descendants. Blur that silhouette, then clip it to the outside
    // of the border box (or the inside of the padding box for inset shadows).
    // Paint back to front: the first CSS shadow is the topmost one.
    for (auto it = shadows.rbegin(); it != shadows.rend(); ++it) {
      const auto& s = *it;
      const float border = s.inset && box.borderSet ? box.borderWidthPx : 0;
      const float w = std::max(0.0f, box.widthPx - 2 * border);
      const float h = std::max(0.0f, box.heightPx - 2 * border);
      if (w <= 0 || h <= 0) continue;
      float radii[] = {std::max(0.0f, box.borderRadiusTLPx - border),
                       std::max(0.0f, box.borderRadiusTRPx - border),
                       std::max(0.0f, box.borderRadiusBRPx - border),
                       std::max(0.0f, box.borderRadiusBLPx - border)};
      const float spread = s.inset ? -s.spread : s.spread;
      const float sw = std::max(0.0f, w + 2 * spread);
      const float sh = std::max(0.0f, h + 2 * spread);
      if (!s.inset && (sw <= 0 || sh <= 0)) continue;
      float expanded[4];
      for (size_t i = 0; i < 4; i++) {
        float adjustment = spread;
        if (!s.inset && spread > 0 && radii[i] < spread) {
          const float ratio = radii[i] / spread;
          const float coverage = std::min(1.0f, 2 * radii[i] / std::max(w, h));
          adjustment *= 1 - std::pow(1 - ratio, 3) * (1 - std::pow(coverage, 3));
        }
        expanded[i] = std::max(0.0f, radii[i] + adjustment);
      }
      // CSS scales overlapping corners by one common factor.
      float scale = 1;
      const float sums[] = {expanded[0] + expanded[1], expanded[3] + expanded[2],
                            expanded[0] + expanded[3], expanded[1] + expanded[2]};
      for (size_t i = 0; i < 4; i++) {
        if (sums[i] > 0) scale = std::min(scale, (i < 2 ? sw : sh) / sums[i]);
      }
      for (auto& radius : expanded) radius *= scale;
      const float extent =
          std::abs(s.offsetX) + std::abs(s.offsetY) + std::abs(spread) + 2 * s.blur + 1;
      auto* sourceData = _document->makeNode<PathData>();
      if (sw > 0 && sh > 0) {
        BuildShadowBoxPath(*sourceData, sw, sh, expanded, box.borderRadiusEllipse);
        sourceData->transform(
            Matrix::Translate(border - spread + s.offsetX, border - spread + s.offsetY));
      }
      auto* clipData = _document->makeNode<PathData>();
      BuildShadowBoxPath(*clipData, w, h, radii, box.borderRadiusEllipse);
      clipData->transform(Matrix::Translate(border, border));
      // An even-odd contour represents the complement without relying on the
      // opacity of the actual element. Its finite outer edge is beyond the
      // blur kernel, so it cannot affect the visible shadow.
      auto* complement = s.inset ? sourceData : clipData;
      complement->moveTo(-extent, -extent);
      complement->lineTo(box.widthPx + extent, -extent);
      complement->lineTo(box.widthPx + extent, box.heightPx + extent);
      complement->lineTo(-extent, box.heightPx + extent);
      complement->close();

      auto* source = _document->makeNode<Layer>();
      auto* mask = _document->makeNode<Layer>();
      mask->id = _idAllocator.generateUnique("shadowClip");
      auto* clipped = _document->makeNode<Layer>();
      auto* group = _document->makeNode<Layer>();
      for (auto* part : {source, mask, clipped, group}) {
        part->includeInLayout = false;
        part->width = box.widthPx;
        part->height = box.heightPx;
        part->left = 0;
        part->top = 0;
      }
      auto* sourcePath = _document->makeNode<Path>();
      sourcePath->data = sourceData;
      auto* sourceFill = buildSolidFill(s.color);
      sourceFill->fillRule = FillRule::EvenOdd;
      source->contents = {sourcePath, sourceFill};
      if (s.blur > 0) {
        auto* blur = _document->makeNode<BlurFilter>();
        blur->blurX = blur->blurY = s.blur * 0.5f;
        source->filters.push_back(blur);
      }
      auto* clipPath = _document->makeNode<Path>();
      clipPath->data = clipData;
      auto* clipFill = buildSolidFill({1, 1, 1, 1, ColorSpace::SRGB});
      clipFill->fillRule = FillRule::EvenOdd;
      mask->contents = {clipPath, clipFill};
      clipped->children.push_back(source);
      clipped->mask = mask;
      clipped->maskType = MaskType::Alpha;
      group->children = {mask, clipped};
      if (s.inset) {
        layer->children.push_back(group);
      } else {
        _outerBoxShadows[layer].push_back(group);
      }
    }
    emitted = true;
    return;
  }
  if (hasSpread) {
    _diagnostics.warn(
        "html: box-shadow spread requires resolved px width/height; "
        "using an unspread shadow");
  }
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
      // A CSS outer `box-shadow` is always clipped to *outside* the element's border box, so it is
      // never visible under the box itself — even when the box has a translucent (or absent)
      // background. PAGX's default `showBehindLayer=true` instead paints the shadow behind the whole
      // layer, so with a translucent fill the shadow bleeds through and washes the box in the shadow
      // colour. Setting `showBehindLayer=false` makes the layer knock the shadow out of its own
      // coverage, matching the CSS clip and leaving only the exterior glow.
      drop->showBehindLayer = false;
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
  // A centered line fragment emitted by html-snapshot is intrinsically sized text inside a
  // fixed-width host. Its measured CSS left/width only describe Chromium's current font metrics;
  // PAGX must measure the editable text itself and keep that result centered as it changes.
  // centerX has higher constraint priority than left, but clear both edge anchors as well so the
  // serialized PAGX expresses the intended layout without stale geometry.
  if (element != nullptr) {
    const auto* intrinsicWidth = element->findAttribute("data-pagx-intrinsic-width");
    if (intrinsicWidth != nullptr && ToLower(Trim(*intrinsicWidth)) == "center") {
      layer->left = NAN;
      layer->right = NAN;
      layer->centerX = 0.0f;
    }
  }
  if (box.opacitySet) layer->alpha = box.opacity;
  if (box.isolate) layer->passThroughBackground = false;
  if (!box.mixBlendMode.empty()) {
    // CSS `mix-blend-mode` uses hyphenated keywords (e.g. `color-dodge`, `hard-light`,
    // `plus-lighter`), which SVGBlendModeFromString maps directly. BlendModeFromString expects
    // PAGX's internal camelCase names and would reject the multi-word CSS values.
    layer->blendMode = SVGBlendModeFromString(box.mixBlendMode);
  }
  if (box.clipOverflow) {
    layer->clipToBounds = true;
    // A zero visible area (width/height: 0 combined with a clipping overflow) renders nothing in a
    // browser. PAGX carries the clip through clipToBounds, which layout later expands into a
    // scrollRect sized from the layer bounds, but an empty scrollRect is treated as "no clipping"
    // by the renderer. Mark the layer invisible here instead of relying on that inference.
    bool zeroArea = (!std::isnan(box.widthPx) && box.widthPx <= 0) ||
                    (!std::isnan(box.heightPx) && box.heightPx <= 0) ||
                    (!std::isnan(box.widthPct) && box.widthPct <= 0) ||
                    (!std::isnan(box.heightPct) && box.heightPct <= 0);
    if (zeroArea) {
      layer->visible = false;
    }
  }

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
      } else if (step.kind == HTMLValueParser::FilterStep::Kind::ColorMatrix) {
        auto cm = _document->makeNode<ColorMatrixFilter>();
        cm->matrix = step.matrix;
        layer->filters.push_back(cm);
      } else if (step.kind == HTMLValueParser::FilterStep::Kind::SvgRef) {
        // `plus-darker` has no CSS mix-blend-mode; the exporter bakes the backdrop into a PNG and
        // applies it through a `pagx_pd_*` feImage+feComposite(arithmetic) filter. That baked filter
        // is a browser-only rendering aid — PAGX blends PlusDarker against the real backdrop, so
        // recover the blend mode from the marker id and drop the filter.
        if (step.refId.compare(0, 8, "pagx_pd_") == 0) {
          layer->blendMode = BlendMode::PlusDarker;
        } else {
          auto decoded = _filterDecoder.decode(step.refId);
          for (auto* f : decoded) {
            layer->filters.push_back(f);
          }
        }
      } else if (step.kind == HTMLValueParser::FilterStep::Kind::ColorMatrix) {
        auto cm = _document->makeNode<ColorMatrixFilter>();
        cm->matrix = step.matrix;
        layer->filters.push_back(cm);
      } else {
        _diagnostics.warn("html: filter '" + step.raw + "' not supported");
      }
    }
  }

  // data-* attributes -> customData
  forwardDataAttributes(layer, element);
  // href on <a>
  if (element->name == "a") {
    auto* href = element->findAttribute("href");
    if (href && !href->empty() && IsValidCustomDataKey("href")) {
      layer->customData["href"] = *href;
    }
  }
}

// data-pagx-* attributes of the PAGX text round-trip protocol are consumed by the importer
// itself (convertElement / HTMLTextFragmentBuilder) and must not surface in Layer::customData.
// data-pagx-version and every other data-* attribute are still forwarded.
static bool IsInternalPagxTextProtocolKey(const std::string& key) {
  static const char* const kProtocolKeys[] = {
      "pagx-text",        "pagx-text-part",        "pagx-font-family",
      "pagx-font-style",  "pagx-letter-spacing",   "pagx-faux-bold",
      "pagx-faux-italic", "pagx-text-orientation", "pagx-inline-positions",
      "pagx-baseline",    "pagx-font-family-list", "pagx-font-variant-emoji",
  };
  for (const char* protocolKey : kProtocolKeys) {
    if (key == protocolKey) {
      return true;
    }
  }
  return false;
}

void HTMLLayerBuilder::forwardDataAttributes(Layer* layer,
                                             const std::shared_ptr<DOMNode>& element) {
  if (layer == nullptr || element == nullptr) return;
  for (const auto& attr : element->attributes) {
    if (attr.name.compare(0, 5, "data-") != 0) continue;
    // Internal html-snapshot/importer handshake; it controls layout semantics and is not user
    // custom data. Keep all other data-* attributes forwarding exactly as before.
    if (attr.name == "data-pagx-intrinsic-width") continue;
    std::string key = attr.name.substr(5);
    if (IsInternalPagxTextProtocolKey(key)) {
      continue;
    }
    if (IsValidCustomDataKey(key)) {
      auto existing = layer->customData.find(key);
      if (existing != layer->customData.end() && existing->second != attr.value) {
        _diagnostics.warn("html: data-* attribute '" + attr.name +
                          "' overrides an existing custom data value");
      }
      layer->customData[key] = attr.value;
    } else {
      _diagnostics.warn("html: invalid data-* attribute name '" + attr.name + "'");
    }
  }
}

Layer* HTMLLayerBuilder::maybeSplitBoxShadowFromClip(Layer* inner) {
  if (inner == nullptr) return inner;
  auto pending = _outerBoxShadows.find(inner);
  const bool hasGeometry = pending != _outerBoxShadows.end();
  if (!hasGeometry && (!inner->clipToBounds || inner->styles.empty())) {
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
  if (hoisted.empty() && !hasGeometry) {
    return inner;
  }

  inner->styles = std::move(kept);

  auto* outer = _document->makeNode<Layer>();

  // Move the parent-facing layout slot (the wrapper occupies the same space in its parent
  // that `inner` previously held). `width`/`height` are also moved here because the shadow-
  // split path lets `inner` fill the wrapper via `percentWidth/percentHeight = 100` below.
  MoveLayoutSlot(outer, inner);
  outer->width = inner->width;
  outer->height = inner->height;

  // Move "through-effects" that semantically wrap the element + its shadow in CSS.
  outer->alpha = inner->alpha;
  outer->blendMode = inner->blendMode;
  outer->passThroughBackground = inner->passThroughBackground;
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
  ResetLayoutAnchors(inner);
  inner->width = NAN;
  inner->height = NAN;
  inner->percentWidth = 100.0f;
  inner->percentHeight = 100.0f;
  inner->alpha = 1.0f;
  inner->blendMode = BlendMode::Normal;
  inner->passThroughBackground = true;
  inner->matrix = Matrix{};
  inner->matrix3D = Matrix3D{};
  inner->preserve3D = false;

  if (hasGeometry) {
    outer->children = std::move(pending->second);
    _outerBoxShadows.erase(pending);
  }
  outer->children.push_back(inner);
  return outer;
}

Layer* HTMLLayerBuilder::wrapForMargin(Layer* inner, const HTMLBoxAttributes& box) {
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

  MoveLayoutSlot(outer, inner);

  ResetLayoutAnchors(inner);
  inner->percentWidth = NAN;
  inner->percentHeight = NAN;

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
