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
  size_t pos = 0;
  while (pos < style.size()) {
    size_t semi = style.find(';', pos);
    std::string decl = style.substr(pos, (semi == std::string::npos ? style.size() : semi) - pos);
    size_t colon = decl.find(':');
    if (colon != std::string::npos) {
      std::string prop = Trim(decl.substr(0, colon));
      std::string value = Trim(decl.substr(colon + 1));
      if (EqualsIgnoreCase(prop, "color") && !value.empty()) {
        return value;
      }
    }
    if (semi == std::string::npos) break;
    pos = semi + 1;
  }
  return {};
}

// Rewrites `fill: currentColor` / `stroke: currentColor` declarations inside an
// inline `style="..."` value to the supplied concrete colour. Other declarations
// (including a leading `color: ...`) are preserved verbatim.
std::string RewriteCurrentColorInInlineStyle(const std::string& style,
                                             const std::string& replacement) {
  std::string out;
  out.reserve(style.size());
  size_t pos = 0;
  bool first = true;
  while (pos < style.size()) {
    size_t semi = style.find(';', pos);
    std::string decl = style.substr(pos, (semi == std::string::npos ? style.size() : semi) - pos);
    size_t colon = decl.find(':');
    if (colon != std::string::npos) {
      std::string prop = Trim(decl.substr(0, colon));
      std::string value = Trim(decl.substr(colon + 1));
      if ((EqualsIgnoreCase(prop, "fill") || EqualsIgnoreCase(prop, "stroke")) &&
          IsCurrentColorKeyword(value)) {
        if (!first) out.push_back(';');
        out += prop;
        out.push_back(':');
        out.push_back(' ');
        out += replacement;
        first = false;
        if (semi == std::string::npos) break;
        pos = semi + 1;
        continue;
      }
    }
    if (!first) out.push_back(';');
    out += decl;
    first = false;
    if (semi == std::string::npos) break;
    pos = semi + 1;
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
  Rectangle* rect = nullptr;
  if (box.backgroundColorSet || !box.backgroundImage.empty() || box.borderRadiusSet ||
      box.borderSet) {
    rect = _document->makeNode<Rectangle>();
    rect->percentWidth = 100.0f;
    rect->percentHeight = 100.0f;
    rect->roundness = box.borderRadiusSet ? box.borderRadiusPx : 0.0f;
    layer->contents.push_back(rect);
    emitted = true;
  }

  // Background colour / gradient.
  if (rect && box.backgroundColorSet && box.backgroundImage.empty()) {
    layer->contents.push_back(buildSolidFill(box.backgroundColor));
    emitted = true;
  } else if (rect && !box.backgroundImage.empty()) {
    auto fill = _document->makeNode<Fill>();
    std::string bg = Trim(box.backgroundImage);
    fill->color = parseGradientByValue(bg);
    if (!fill->color) {
      std::string lower = ToLower(bg);
      if (lower.compare(0, 4, "url(") == 0) {
        warn("html: background-image '" + bg + "' (url) not supported; use <img>");
      } else {
        warn("html: background-image '" + bg + "' not supported");
      }
    }
    if (!fill->color && box.backgroundColorSet) {
      auto solid = _document->makeNode<SolidColor>();
      solid->color = box.backgroundColor;
      fill->color = solid;
    }
    if (fill->color) {
      layer->contents.push_back(fill);
      emitted = true;
    }
  }

  // Border.
  if (rect && box.borderSet) {
    auto stroke = _document->makeNode<Stroke>();
    auto solid = _document->makeNode<SolidColor>();
    solid->color = box.borderColor;
    stroke->color = solid;
    stroke->width = box.borderWidthPx;
    stroke->align = StrokeAlign::Inside;
    layer->contents.push_back(stroke);
    emitted = true;
  }

  // box-shadow -> DropShadowStyle / InnerShadowStyle list.
  if (!box.boxShadow.empty()) {
    auto shadows = parseShadowList(box.boxShadow);
    for (auto& s : shadows) {
      if (s.inset) {
        auto inner = _document->makeNode<InnerShadowStyle>();
        inner->offsetX = s.offsetX;
        inner->offsetY = s.offsetY;
        inner->blurX = s.blur;
        inner->blurY = s.blur;
        inner->color = s.color;
        layer->styles.push_back(inner);
      } else {
        auto drop = _document->makeNode<DropShadowStyle>();
        drop->offsetX = s.offsetX;
        drop->offsetY = s.offsetY;
        drop->blurX = s.blur;
        drop->blurY = s.blur;
        drop->color = s.color;
        layer->styles.push_back(drop);
      }
    }
    if (!shadows.empty()) emitted = true;
  }

  // backdrop-filter: blur(...) -> BackgroundBlurStyle.
  if (!box.backdropFilter.empty()) {
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

  return emitted;
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

  auto rect = _document->makeNode<Rectangle>();
  rect->percentWidth = 100.0f;
  rect->percentHeight = 100.0f;
  layer->contents.push_back(rect);

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
