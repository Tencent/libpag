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

#include <algorithm>
#include <cmath>
#include <string>
#include "base/utils/MathUtil.h"
#include "pagx/html/HTMLWriter.h"
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLParserContext.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

using namespace pagx::html;

namespace {

void SerializeNode(std::string& out, const std::shared_ptr<DOMNode>& node, int depth) {
  if (!node) return;
  if (depth >= MAX_HTML_RECURSION_DEPTH) return;
  if (node->type == DOMNodeType::Text) {
    out += EscapeXml(node->name, /*isAttribute=*/false);
    return;
  }
  out.push_back('<');
  out += node->name;
  for (const auto& attr : node->attributes) {
    out.push_back(' ');
    out += attr.name;
    out += "=\"";
    out += EscapeXml(attr.value, /*isAttribute=*/true);
    out.push_back('"');
  }
  if (!node->getFirstChild()) {
    out += "/>";
    return;
  }
  out.push_back('>');
  auto child = node->getFirstChild();
  while (child) {
    SerializeNode(out, child, depth + 1);
    child = child->getNextSibling();
  }
  out += "</";
  out += node->name;
  out.push_back('>');
}

// SVG `fill` / `stroke` attributes accept CSS colour tokens. PAGX's SVG importer
// understands `#RRGGBB`, `#RRGGBBAA`, and `rgb()/rgba()`, but not the CSS `color()`
// function used for DisplayP3 — `ColorToSVGHex` collapses those to sRGB hex for us.
// Alpha < 1 is rare for `currentColor` resolution (CSS `color` rarely carries
// transparency), but we round-trip it via the 8-digit hex form when present so the
// importer's `addFillStroke` path picks up the right alpha.
std::string FormatColorForSvgAttribute(const Color& color) {
  if (color.alpha >= 1.0f || color.colorSpace == ColorSpace::DisplayP3) {
    return ColorToSVGHex(color);
  }
  int r = 0;
  int g = 0;
  int b = 0;
  ColorToRGB(color, r, g, b);
  int a = std::clamp(static_cast<int>(std::round(color.alpha * 255.0f)), 0, 255);
  char buf[10] = {};
  snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", r, g, b, a);
  return buf;
}

// Case-insensitive equality for ASCII property/keyword names. SVG attribute
// names like `color` and CSS keywords like `currentColor` are matched here.
bool EqualsIgnoreCase(const std::string& a, const char* literal) {
  size_t i = 0;
  for (; i < a.size() && literal[i] != '\0'; ++i) {
    char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
    char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(literal[i])));
    if (ca != cb) return false;
  }
  return i == a.size() && literal[i] == '\0';
}

// CSS `currentColor` (any casing) is the spelling we resolve here. The icon-font
// pre-pass in tools/html-snapshot emits `fill="currentColor"` verbatim, and authored
// inline SVGs sometimes use `currentcolor` lowercased.
bool IsCurrentColorKeyword(const std::string& value) {
  return EqualsIgnoreCase(value, "currentColor") || EqualsIgnoreCase(value, "currentcolor");
}

// Extracts a `color: <value>` declaration from an inline `style="..."` string,
// returning the trimmed value or empty when absent. Other properties (e.g.
// `fill-color`, `background-color`) are ignored — we only honour the bare `color`
// keyword, which is what CSS uses for `currentColor` cascade.
std::string ExtractColorFromInlineStyle(const std::string& style) {
  for (const auto& decl : ParseStyleDeclarations(style)) {
    if (EqualsIgnoreCase(decl.first, "color") && !decl.second.empty()) {
      return decl.second;
    }
  }
  return {};
}

// Rewrites `fill: currentColor` / `stroke: currentColor` declarations inside an
// inline `style="..."` value to the supplied concrete colour. Other declarations
// (including a leading `color: ...`) are preserved verbatim.
std::string RewriteCurrentColorInInlineStyle(const std::string& style,
                                             const std::string& replacement) {
  auto decls = ParseStyleDeclarations(style);
  std::string out;
  out.reserve(style.size());
  bool first = true;
  for (auto& decl : decls) {
    std::string value = decl.second;
    if ((EqualsIgnoreCase(decl.first, "fill") || EqualsIgnoreCase(decl.first, "stroke")) &&
        IsCurrentColorKeyword(value)) {
      value = replacement;
    }
    if (!first) out.push_back(';');
    out += decl.first;
    out.push_back(':');
    out.push_back(' ');
    out += value;
    first = false;
  }
  return out;
}

// Walks an inline SVG subtree and replaces `currentColor` references in `fill` /
// `stroke` (attribute form plus inline `style="…"` form) with the concrete
// colour active at that node. CSS cascade for the `color` property is honoured
// inside the subtree: an SVG-internal element's `color` attribute or
// `style="color: …"` declaration overrides the inherited value for itself and
// its descendants.
//
// The root SVG element is handled specially: the caller pre-resolves its
// effective colour via `computeInherited` (which already folds any inline
// `style="color:…"` the HTML cascade pass attached to the SVG), and passes
// that as `inheritedColor`. We deliberately skip re-reading the root's own
// `color` / `style` to preserve the caller-supplied formatting (e.g. so a
// snapshot-stage `color: rgb(...)` round-trips as the hex form we resolved
// from the parsed `Color`). Descendants get the standard cascade tracking.
//
// This runs before serialising the SVG into a PAGX import directive because
// the downstream SVG importer has no notion of `color` / `currentColor` — by
// the time it parses the directive content, every fill/stroke must already
// carry a concrete colour token. Mirrors browser-snapshot's `freezeSvg`
// (tools/html-snapshot/lib/browser-snapshot.js) but runs at C++ import time
// rather than during the browser snapshot, so that icon-font glyphs (which
// the snapshot intentionally leaves as `fill="currentColor"` so the wrapper's
// `color` flows through at render time) still pick up the right tint.
void ResolveCurrentColorInSvg(const std::shared_ptr<DOMNode>& node,
                              const std::string& inheritedColor, int depth, bool isRoot) {
  if (!node || depth >= MAX_HTML_RECURSION_DEPTH) return;
  if (node->type != DOMNodeType::Element) return;

  std::string here = inheritedColor;
  if (!isRoot) {
    for (const auto& attr : node->attributes) {
      if (EqualsIgnoreCase(attr.name, "color")) {
        std::string value = Trim(attr.value);
        if (!value.empty() && !EqualsIgnoreCase(value, "inherit") &&
            !IsCurrentColorKeyword(value)) {
          here = value;
        }
      } else if (EqualsIgnoreCase(attr.name, "style")) {
        std::string colorFromStyle = ExtractColorFromInlineStyle(attr.value);
        if (!colorFromStyle.empty() && !EqualsIgnoreCase(colorFromStyle, "inherit") &&
            !IsCurrentColorKeyword(colorFromStyle)) {
          here = colorFromStyle;
        }
      }
    }
  }

  for (auto& attr : node->attributes) {
    if ((EqualsIgnoreCase(attr.name, "fill") || EqualsIgnoreCase(attr.name, "stroke")) &&
        IsCurrentColorKeyword(attr.value)) {
      attr.value = here;
    } else if (EqualsIgnoreCase(attr.name, "style")) {
      attr.value = RewriteCurrentColorInInlineStyle(attr.value, here);
    }
  }

  for (auto child = node->getFirstChild(); child; child = child->getNextSibling()) {
    ResolveCurrentColorInSvg(child, here, depth + 1, /*isRoot=*/false);
  }
}

// External `.svg` URL — i.e. ends with `.svg` and is not an inline `data:` URI. Such
// sources route through an SVG import directive rather than a raster image fill.
bool IsExternalSvgSrc(const std::string& src) {
  if (src.size() <= 4) return false;
  if (src.compare(0, 5, "data:") == 0) return false;
  return ToLower(src.substr(src.size() - 4)) == ".svg";
}

// Maps a CSS `object-fit` keyword onto a PAGX `ScaleMode`. Empty input means
// "unset" and yields the CSS default `fill` -> `Stretch` so a plain `<img>` with
// explicit width/height matches the browser's stretch behaviour rather than the
// `ImagePattern` default `LetterBox` (which preserves aspect ratio and would
// shrink images whose intrinsic aspect differs from the box's).
ScaleMode ResolveImageScaleMode(const std::string& objectFit) {
  if (objectFit.empty() || objectFit == "fill") return ScaleMode::Stretch;
  if (objectFit == "contain") return ScaleMode::LetterBox;
  if (objectFit == "cover") return ScaleMode::Zoom;
  // The SubsetTransformer normalises any other keyword to one of the above before
  // emitting the subset HTML; reaching this branch means a hand-written subset
  // file slipped through. Fall back to the CSS default.
  return ScaleMode::Stretch;
}

// Traces the outline of a rounded rectangle with per-corner radii (TL, TR, BR, BL) inside the
// axis-aligned box (0, 0)-(W, H) onto `out`. Going clockwise starting at the top of the right
// edge — matches the canonical PAGX rendering start point for rounded rectangles, so the path
// reverses / orientation logic in shape modifiers (`ApplyRoundCorner`, `MergePath`, etc.) sees
// the same winding it would from a uniform Rectangle. Corners with radius 0 collapse to a sharp
// vertex (no cubic emitted). Caller is responsible for clamping the radii so adjacent pairs do
// not exceed the box's edge length — see the CSS scaling clamp in `HTMLStyleResolver`.
void BuildPerCornerRoundedRectPathData(PathData& out, float w, float h, float tl, float tr,
                                       float brad, float bl) {
  const float k = 1.0f - BEZIER_KAPPA;
  // Start at the top of the right edge (or the top-right corner when TR == 0). Mirrors the
  // Rectangle-with-uniform-roundness writer in HTMLWriterShape.cpp so existing geometry tests
  // (and any downstream RoundCorner modifier that relies on the start vertex landing on an
  // edge) keep working.
  out.moveTo(w, tr);
  // Right edge.
  out.lineTo(w, h - brad);
  if (brad > 0) {
    // BR corner: cubic from (W, H - BR) to (W - BR, H), bulging into (W, H).
    out.cubicTo(w, h - brad * k, w - brad * k, h, w - brad, h);
  }
  // Bottom edge.
  out.lineTo(bl, h);
  if (bl > 0) {
    // BL corner: cubic from (BL, H) to (0, H - BL), bulging into (0, H).
    out.cubicTo(bl * k, h, 0, h - bl * k, 0, h - bl);
  }
  // Left edge.
  out.lineTo(0, tl);
  if (tl > 0) {
    // TL corner: cubic from (0, TL) to (TL, 0), bulging into (0, 0).
    out.cubicTo(0, tl * k, tl * k, 0, tl, 0);
  }
  // Top edge.
  out.lineTo(w - tr, 0);
  if (tr > 0) {
    // TR corner: cubic from (W - TR, 0) back to (W, TR), bulging into (W, 0). Closes the loop.
    out.cubicTo(w - tr * k, 0, w, tr * k, w, tr);
  }
  out.close();
}

}  // namespace

Text* HTMLParserContext::buildTextElement(const TextFragment& fragment) {
  auto t = _document->makeNode<Text>();
  t->text = fragment.text;
  t->fontFamily = fragment.fontFamily;
  t->fontStyle = fragment.fontStyleName;
  t->fontSize = fragment.fontSize;
  t->letterSpacing = fragment.letterSpacing;
  return t;
}

Fill* HTMLParserContext::buildSolidFill(const Color& color) {
  auto fill = _document->makeNode<Fill>();
  auto solid = _document->makeNode<SolidColor>();
  solid->color = color;
  fill->color = solid;
  return fill;
}

Fill* HTMLParserContext::buildTextFill(const TextFragment& fragment) {
  if (fragment.fillImage.empty()) {
    return buildSolidFill(fragment.color);
  }
  auto fill = _document->makeNode<Fill>();
  fill->color = parseGradientByValue(fragment.fillImage);
  if (!fill->color) {
    // Unparseable gradient: fall back to solid color so text stays visible. This mirrors
    // what `applyBackgroundVisuals` does for an unparseable box gradient.
    return buildSolidFill(fragment.color);
  }
  return fill;
}

ColorSource* HTMLParserContext::parseGradientByValue(const std::string& value) {
  std::string trimmed = Trim(value);
  if (trimmed.empty()) return nullptr;
  std::string lower = ToLower(trimmed);
  if (lower.compare(0, 16, "linear-gradient(") == 0) {
    return parseLinearGradient(trimmed);
  }
  if (lower.compare(0, 16, "radial-gradient(") == 0) {
    return parseRadialGradient(trimmed);
  }
  if (lower.compare(0, 15, "conic-gradient(") == 0) {
    return parseConicGradient(trimmed);
  }
  return nullptr;
}

void HTMLParserContext::applySizeAndPosition(Layer* layer, const HTMLBoxAttributes& box) {
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

void HTMLParserContext::applyLayoutAttributes(Layer* layer, const HTMLBoxAttributes& box) {
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
      warn("html: unsupported align-items '" + box.alignItems + "'");
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
      warn("html: unsupported justify-content '" + box.justifyContent + "'");
    }
  }
}

Element* HTMLParserContext::buildBackgroundGeometry(const HTMLBoxAttributes& box) {
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
    warn(
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

bool HTMLParserContext::applyBackgroundVisuals(Layer* layer, const HTMLBoxAttributes& box) {
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

void HTMLParserContext::applyBackgroundFill(Layer* layer, const HTMLBoxAttributes& box,
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
        warn("html: background-image '" + part + "' (url) not supported; use <img>");
      } else {
        warn("html: background-image '" + part + "' not supported");
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

void HTMLParserContext::applyBorderStroke(Layer* layer, const HTMLBoxAttributes& box,
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

void HTMLParserContext::applyBoxShadows(Layer* layer, const HTMLBoxAttributes& box, bool& emitted) {
  if (box.boxShadow.empty()) return;
  // CSS spec defines the `box-shadow` blur-radius as the standard deviation of the Gaussian
  // blur, doubled. PAGX layer-style `blurX/blurY` values flow into tgfx's `ImageFilter::Blur`
  // (and SVG `feGaussianBlur stdDeviation`), both of which take Gaussian σ directly. Halving
  // the parsed blur-radius lands on the matching σ. NOTE: this conversion is specific to
  // `box-shadow` — the `filter: drop-shadow()` consumer keeps its blur as-is, since the CSS
  // Filter spec defines that function's blur-radius as σ already.
  auto shadows = parseShadowList(box.boxShadow);
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

void HTMLParserContext::applyBackdropFilter(Layer* layer, const HTMLBoxAttributes& box,
                                            bool& emitted) {
  if (box.backdropFilter.empty()) return;
  auto steps = parseFilterChain(box.backdropFilter);
  for (auto& step : steps) {
    if (step.kind == FilterStep::Kind::Blur) {
      auto bg = _document->makeNode<BackgroundBlurStyle>();
      bg->blurX = step.blurX;
      bg->blurY = step.blurY;
      layer->styles.push_back(bg);
      emitted = true;
    } else {
      warn("html: backdrop-filter '" + step.raw + "' not supported");
    }
  }
}

void HTMLParserContext::applyBoxTransform(Layer* layer, const HTMLBoxAttributes& box,
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
    warn("html: transform on '<" + tag +
         ">' without explicit width/height; transform-origin defaults to top-left and "
         "may differ from CSS transform-origin: 50% 50%");
  }
  if (!std::isnan(cx) && !std::isnan(cy)) {
    Matrix toCenter = Matrix::Translate(cx, cy);
    Matrix fromCenter = Matrix::Translate(-cx, -cy);
    m = toCenter * m * fromCenter;
  }
  layer->matrix = m;
}

void HTMLParserContext::applyLayerAttributes(Layer* layer, const std::shared_ptr<DOMNode>& element,
                                             const HTMLBoxAttributes& box) {
  if (box.opacitySet) layer->alpha = box.opacity;
  if (!box.mixBlendMode.empty()) {
    BlendMode mode = BlendModeFromString(box.mixBlendMode);
    layer->blendMode = mode;
  }
  if (box.clipOverflow) layer->clipToBounds = true;

  // filter chain (excluding backdrop-filter, which is handled as a Layer style).
  if (!box.filter.empty()) {
    auto steps = parseFilterChain(box.filter);
    for (auto& step : steps) {
      if (step.kind == FilterStep::Kind::Blur) {
        auto blur = _document->makeNode<BlurFilter>();
        blur->blurX = step.blurX;
        blur->blurY = step.blurY;
        layer->filters.push_back(blur);
      } else if (step.kind == FilterStep::Kind::DropShadow) {
        auto drop = _document->makeNode<DropShadowFilter>();
        drop->offsetX = step.shadow.offsetX;
        drop->offsetY = step.shadow.offsetY;
        drop->blurX = step.shadow.blur;
        drop->blurY = step.shadow.blur;
        drop->color = step.shadow.color;
        layer->filters.push_back(drop);
      } else {
        warn("html: filter '" + step.raw + "' not supported");
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
        warn("html: invalid data-* attribute name '" + attr.name + "'");
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

Layer* HTMLParserContext::maybeSplitBoxShadowFromClip(Layer* inner) {
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

Layer* HTMLParserContext::wrapWithMargin(Layer* inner, const HTMLBoxAttributes& box) {
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

bool HTMLParserContext::foldRoundedImageWrapper(const std::shared_ptr<DOMNode>& element,
                                                const HTMLBoxAttributes& box, Layer* layer) {
  if (!box.borderRadiusSet || !box.clipOverflow) return false;
  if (requiresInnerHost(box)) return false;

  std::shared_ptr<DOMNode> img = nullptr;
  for (auto c = element->getFirstChild(); c; c = c->getNextSibling()) {
    if (c->type == DOMNodeType::Element) {
      if (img) return false;
      if (c->name != "img") return false;
      img = c;
    } else if (c->type == DOMNodeType::Text) {
      if (!IsBlankText(c->name)) return false;
    }
  }
  if (!img) return false;

  // Reject SVG sources up front: they ride an import directive, not a raster fill.
  auto* srcAttr = img->findAttribute("src");
  if (!srcAttr || srcAttr->empty()) return false;
  const std::string& src = *srcAttr;
  if (IsExternalSvgSrc(src)) return false;

  // The image must exactly cover the wrapper's content box, anchored at top-left —
  // otherwise the rounded clip would shape only part of the visible image and folding
  // would stretch it across the wrapper.
  HTMLBoxAttributes imgBox = resolveBox(img);
  if (std::isnan(imgBox.widthPx) || std::isnan(box.widthPx) ||
      !pag::FloatNearlyEqual(imgBox.widthPx, box.widthPx, HTML_IMAGE_WRAPPER_TOLERANCE_PX)) {
    return false;
  }
  if (std::isnan(imgBox.heightPx) || std::isnan(box.heightPx) ||
      !pag::FloatNearlyEqual(imgBox.heightPx, box.heightPx, HTML_IMAGE_WRAPPER_TOLERANCE_PX)) {
    return false;
  }
  float imgLeft = std::isnan(imgBox.leftPx) ? 0.0f : imgBox.leftPx;
  float imgTop = std::isnan(imgBox.topPx) ? 0.0f : imgBox.topPx;
  if (!pag::FloatNearlyEqual(imgLeft, 0.0f, HTML_IMAGE_WRAPPER_TOLERANCE_PX) ||
      !pag::FloatNearlyEqual(imgTop, 0.0f, HTML_IMAGE_WRAPPER_TOLERANCE_PX)) {
    return false;
  }

  auto* imageNode = registerImageResource(resolveImageSource(src));
  if (!imageNode) return false;

  // The rounded Rectangle emitted by applyBackgroundVisuals() above is now the actual
  // fill geometry, so the rectangular clip the importer would have written from
  // `overflow: hidden` is redundant.
  layer->clipToBounds = false;

  auto fill = _document->makeNode<Fill>();
  auto pattern = _document->makeNode<ImagePattern>();
  pattern->image = imageNode;
  pattern->scaleMode = ResolveImageScaleMode(imgBox.objectFit);
  fill->color = pattern;
  layer->contents.push_back(fill);
  assignElementId(layer, element);
  return true;
}

Layer* HTMLParserContext::convertImage(const std::shared_ptr<DOMNode>& element,
                                       const HTMLBoxAttributes& box) {
  auto* srcAttr = element->findAttribute("src");
  if (!srcAttr || srcAttr->empty()) {
    warn("html: <img> missing src; skipped");
    return nullptr;
  }
  const std::string& src = *srcAttr;
  if (IsExternalSvgSrc(src)) {
    auto layer = _document->makeNode<Layer>();
    applySizeAndPosition(layer, box);
    applyLayerAttributes(layer, element, box);
    layer->importDirective.source = resolveImageSource(src);
    layer->importDirective.format = "svg";
    assignElementId(layer, element);
    return layer;
  }

  auto* imageNode = registerImageResource(resolveImageSource(src));
  if (!imageNode) return nullptr;

  auto layer = _document->makeNode<Layer>();
  applySizeAndPosition(layer, box);
  applyLayerAttributes(layer, element, box);

  // Honour `border-radius` directly on `<img>`: a CSS `<img style="border-radius: 50%">` is the
  // canonical way to draw a circular avatar, and per-corner radii (e.g. only the top corners
  // rounded for a "card cover" image) follow the same Path-emission path the container code
  // uses. When the image has no border-radius, `buildBackgroundGeometry` falls back to a plain
  // `Rectangle width=100% height=100%`, preserving the legacy emission verbatim.
  layer->contents.push_back(buildBackgroundGeometry(box));

  auto fill = _document->makeNode<Fill>();
  auto pattern = _document->makeNode<ImagePattern>();
  pattern->image = imageNode;
  pattern->scaleMode = ResolveImageScaleMode(box.objectFit);
  fill->color = pattern;
  layer->contents.push_back(fill);
  assignElementId(layer, element);
  return layer;
}

Layer* HTMLParserContext::convertInlineSvg(const std::shared_ptr<DOMNode>& element,
                                           const HTMLBoxAttributes& box,
                                           const HTMLInheritedStyle& inherited) {
  auto layer = _document->makeNode<Layer>();
  applySizeAndPosition(layer, box);
  applyLayerAttributes(layer, element, box);

  // CSS `color` cascades into the SVG and is what `currentColor` resolves to.
  // `computeInherited` returns the style descendants see, but `resolvedTextColor`
  // is the *element's own* colour after applying any `style="color: …"` on the
  // SVG itself — exactly what `currentColor` should resolve to at the SVG root.
  // We pre-resolve here (mutating the captured subtree in place) because the
  // downstream SVG importer treats anything other than a literal colour token
  // as black, dropping the wrapper's tint that the snapshot relied on for
  // icon-font glyphs.
  HTMLInheritedStyle svgStyle = computeInherited(element, inherited);
  std::string rootColor = FormatColorForSvgAttribute(svgStyle.resolvedTextColor);
  ResolveCurrentColorInSvg(element, rootColor, /*depth=*/0, /*isRoot=*/true);

  layer->importDirective.content = serializeSvg(element);
  layer->importDirective.format = "svg";
  assignElementId(layer, element);
  return layer;
}

std::string HTMLParserContext::serializeSvg(const std::shared_ptr<DOMNode>& svgNode) {
  std::string out;
  SerializeNode(out, svgNode, 0);
  return out;
}

Image* HTMLParserContext::registerImageResource(const std::string& imageSource) {
  if (imageSource.empty()) return nullptr;
  auto it = _imageSourceToId.find(imageSource);
  if (it != _imageSourceToId.end()) return it->second;
  auto imageNode = _document->makeNode<Image>();
  imageNode->id = generateUniqueId("image");
  imageNode->filePath = imageSource;
  _imageSourceToId[imageSource] = imageNode;
  return imageNode;
}

std::string HTMLParserContext::resolveImageSource(const std::string& src) const {
  return LooksAbsolutePath(src) ? src : (_basePath + src);
}

void HTMLParserContext::emitTextDecorationLine(Layer* host, const Color& /*textColor*/,
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
