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

// HTMLParserContext methods that convert specific HTML elements (`<img>`, inline `<svg>`, the
// rounded-image-wrapper fold) into PAGX `Layer` instances. Pure layer-side helpers live in
// `HTMLLayerBuilder` and are reached through `_layerBuilder` here. Text-fragment building is
// handled by `HTMLTextFragmentBuilder` and reached through `_textFragmentBuilder`.

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include "base/utils/MathUtil.h"
#include "pagx/SVGImporter.h"
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLParserContext.h"
#include "pagx/html/importer/HTMLValueParser.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/svg/SVGPathParser.h"
#include "pagx/types/MaskType.h"
#include "pagx/utils/Base64.h"
#include "pagx/utils/StringParser.h"
#include "pagx/xml/XMLDOM.h"
#include "renderer/ToTGFX.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/PathMeasure.h"

namespace pagx {

using namespace pagx::html;

namespace {

// External `.svg` URL — i.e. ends with `.svg` and is not an inline `data:` URI. Such
// sources route through an SVG import directive rather than a raster image fill.
bool IsExternalSvgSrc(const std::string& src) {
  if (src.size() <= 4) return false;
  if (src.compare(0, 5, "data:") == 0) return false;
  return ToLower(src.substr(src.size() - 4)) == ".svg";
}

// Resolves one axis of a per-axis `mask-size` token into an on-element pixel length. A length
// (`120px`) is taken verbatim; a percentage resolves against the element's box axis; `auto` (and
// any unparseable token) returns NaN so the caller can tie the axis to the other for aspect-ratio
// preservation. `intrinsicAxis` is unused for px/% but kept in the signature for symmetry with the
// CSS model where `auto` would otherwise fall back to the intrinsic size.
float ResolveMaskSizeAxis(const std::string& token, float boxAxis, float /*intrinsicAxis*/,
                          HTMLValueParser& parser) {
  if (token.empty() || token == "auto") return NAN;
  float fraction = 0;
  if (ParseCssPercentage(token, fraction)) {
    return fraction * boxAxis;
  }
  return parser.parseAbsoluteLengthPx(token);
}

// Maximum depth of layout-only `<div>` wrappers `foldRoundedImageWrapper` will skip
// past while searching for the inner `<img>`. Three is enough to cover the
// stacked wrappers the html-snapshot pipeline emits for replaced elements while
// keeping the search bounded.
constexpr int HTML_ROUND_IMAGE_MAX_WRAPPER_DEPTH = 3;

// Returns the single non-blank-text element child of `parent` (skipping blank text
// nodes), or nullptr when `parent` has zero or more than one element child, or any
// non-blank text content. Used by `foldRoundedImageWrapper` to walk past intermediate
// layout-only wrapper divs the html-snapshot pipeline inserts between an authored
// `<div border-radius;overflow:hidden>` and the inner `<img>`.
std::shared_ptr<DOMNode> FindSoleElementChild(const std::shared_ptr<DOMNode>& parent) {
  std::shared_ptr<DOMNode> only = nullptr;
  for (auto c = parent->getFirstChild(); c; c = c->getNextSibling()) {
    if (c->type == DOMNodeType::Element) {
      if (only) return nullptr;
      only = c;
    } else if (c->type == DOMNodeType::Text) {
      if (!IsBlankText(c->name)) return nullptr;
    }
  }
  return only;
}

// Tests whether `box` describes a `<div>` whose only role is to forward layout to its
// child — i.e. it has no painting effect on its own. Such intermediate wrappers must be
// stepped through by `foldRoundedImageWrapper` so a fold that conceptually applies the
// outer container's rounded clip to the inner `<img>` is not blocked when the
// html-snapshot pipeline (or hand-written markup) sandwiches a transparent positioning
// `<div>` between them.
bool IsLayoutOnlyWrapperBox(const HTMLBoxAttributes& box) {
  if (box.backgroundColorSet) return false;
  if (!box.backgroundImage.empty()) return false;
  if (box.borderRadiusSet) return false;
  if (box.borderSet) return false;
  if (!box.boxShadow.empty()) return false;
  if (!box.filter.empty()) return false;
  if (!box.backdropFilter.empty()) return false;
  if (box.opacitySet && box.opacity < 1.0f) return false;
  if (!box.mixBlendMode.empty()) return false;
  if (box.transform.valid) return false;
  if (HTMLLayerBuilder::requiresInnerHost(box)) return false;
  return true;
}

// Wrapper geometry must exactly cover `outer`'s content box, anchored at the top-left
// corner with no offset of its own. Mirrors the `<img>`-side check at the leaf of the
// fold so an intermediate `<div>` cannot quietly resize or offset the image relative to
// the rounded outline emitted from the outer container.
bool WrapperBoxCoversOuter(const HTMLBoxAttributes& wrapperBox, const HTMLBoxAttributes& outerBox) {
  if (std::isnan(wrapperBox.widthPx) || std::isnan(outerBox.widthPx) ||
      !pag::FloatNearlyEqual(wrapperBox.widthPx, outerBox.widthPx,
                             HTML_IMAGE_WRAPPER_TOLERANCE_PX)) {
    return false;
  }
  if (std::isnan(wrapperBox.heightPx) || std::isnan(outerBox.heightPx) ||
      !pag::FloatNearlyEqual(wrapperBox.heightPx, outerBox.heightPx,
                             HTML_IMAGE_WRAPPER_TOLERANCE_PX)) {
    return false;
  }
  float left = std::isnan(wrapperBox.leftPx) ? 0.0f : wrapperBox.leftPx;
  float top = std::isnan(wrapperBox.topPx) ? 0.0f : wrapperBox.topPx;
  if (!pag::FloatNearlyEqual(left, 0.0f, HTML_IMAGE_WRAPPER_TOLERANCE_PX) ||
      !pag::FloatNearlyEqual(top, 0.0f, HTML_IMAGE_WRAPPER_TOLERANCE_PX)) {
    return false;
  }
  return true;
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

// Extracts the source string from a CSS `url(...)` token, stripping the wrapper, surrounding
// whitespace, and matching quotes. Returns empty when `value` is not a `url(...)` form.
std::string ExtractCssUrl(const std::string& value) {
  std::string trimmed = Trim(value);
  std::string lower = ToLower(trimmed);
  auto open = lower.find("url(");
  if (open == std::string::npos) return {};
  auto close = trimmed.rfind(')');
  if (close == std::string::npos || close <= open + 4) return {};
  std::string inner = Trim(trimmed.substr(open + 4, close - (open + 4)));
  if (inner.size() >= 2 && (inner.front() == '"' || inner.front() == '\'') &&
      inner.back() == inner.front()) {
    inner = inner.substr(1, inner.size() - 2);
  }
  std::string unescaped;
  unescaped.reserve(inner.size());
  for (size_t i = 0; i < inner.size(); ++i) {
    if (inner[i] == '\\' && i + 1 < inner.size() &&
        (inner[i + 1] == '\\' || inner[i + 1] == '\'' || inner[i + 1] == '"')) {
      unescaped.push_back(inner[++i]);
    } else {
      unescaped.push_back(inner[i]);
    }
  }
  return Trim(unescaped);
}

// Decodes a single hex digit to its 0-15 value, or -1 when not a hex digit.
int HexDigitValue(char h) {
  if (h >= '0' && h <= '9') return h - '0';
  if (h >= 'a' && h <= 'f') return h - 'a' + 10;
  if (h >= 'A' && h <= 'F') return h - 'A' + 10;
  return -1;
}

// Decodes a percent-encoded `data:image/svg+xml,...` URI payload into raw SVG text. The HTML
// exporter emits the mask SVG as `url('data:image/svg+xml,<percent-encoded>')` (NOT base64), so
// only the `%XX` escapes the exporter produced (`<`, `>`, `#`, `"`, `'`) plus any stray ones need
// undoing. Returns empty when `dataUri` is not an `image/svg+xml` data URI. A `base64,` payload is
// rejected (the exporter never emits one) rather than mis-decoded.
std::string DecodeSvgDataUri(const std::string& dataUri) {
  static const char* Prefix = "data:image/svg+xml";
  if (dataUri.compare(0, std::strlen(Prefix), Prefix) != 0) return {};
  auto comma = dataUri.find(',');
  if (comma == std::string::npos) return {};
  std::string meta = dataUri.substr(std::strlen(Prefix), comma - std::strlen(Prefix));
  if (ToLower(meta).find("base64") != std::string::npos) return {};
  std::string out;
  out.reserve(dataUri.size() - comma);
  for (size_t i = comma + 1; i < dataUri.size(); ++i) {
    char c = dataUri[i];
    if (c == '%' && i + 2 < dataUri.size()) {
      int hi = HexDigitValue(dataUri[i + 1]);
      int lo = HexDigitValue(dataUri[i + 2]);
      if (hi >= 0 && lo >= 0) {
        out.push_back(static_cast<char>((hi << 4) | lo));
        i += 2;
        continue;
      }
    }
    out.push_back(c);
  }
  return out;
}

// Maps a `background-repeat` keyword onto a per-axis tile mode. CSS `repeat` / `repeat-x` /
// `repeat-y` tile; `no-repeat` / `space` / `round` clamp to Decal so the single image is painted
// once. An empty value falls back to the CSS default `repeat` (both axes tile). The exporter only
// ever emits `repeat` / `no-repeat` (optionally per-axis), which this covers exactly.
void ResolveBackgroundRepeat(const std::string& repeat, TileMode& outX, TileMode& outY) {
  if (repeat.empty()) {
    outX = TileMode::Repeat;
    outY = TileMode::Repeat;
    return;
  }
  outX = TileMode::Decal;
  outY = TileMode::Decal;
  auto tokens = SplitTopLevelWhitespace(repeat);
  std::string x = tokens.empty() ? std::string() : tokens[0];
  std::string y = tokens.size() > 1 ? tokens[1] : x;
  if (x == "repeat") outX = TileMode::Repeat;
  if (y == "repeat") outY = TileMode::Repeat;
  // `repeat-x` / `repeat-y` are single-axis shorthands: a leading `repeat-x` tiles X only.
  if (x == "repeat-x") {
    outX = TileMode::Repeat;
    outY = TileMode::Decal;
  } else if (x == "repeat-y") {
    outX = TileMode::Decal;
    outY = TileMode::Repeat;
  }
}

// Reads a DOM element attribute by (case-insensitive) name, or empty when absent. `name` must be
// supplied lowercased. Inline SVG attribute names keep their authored casing (`clipPathUnits`), so
// the comparison lowercases the stored name before matching.
std::string GetDomAttribute(const std::shared_ptr<DOMNode>& node, const std::string& name) {
  if (node == nullptr) return {};
  for (const auto& attr : node->attributes) {
    if (ToLower(attr.name) == name) return attr.value;
  }
  return {};
}

// Resolves a CSS `<length-percentage>` clip coordinate against `axis` (the box width or height in
// px). Percentages resolve against the axis; unitless (only `0` is valid unitless in CSS, but any
// number is accepted leniently) and `px` lengths are taken verbatim. Returns NaN for anything else
// (calc(), unsupported units) so the caller can drop the clip-path with a diagnostic.
float ResolveClipCoord(const std::string& token, float axis) {
  std::string t = Trim(token);
  if (t.empty()) return NAN;
  float fraction = 0;
  if (ParseCssPercentage(t, fraction)) return fraction * axis;
  const char* c = t.c_str();
  char* end = nullptr;
  float num = std::strtof(c, &end);
  if (end == c) return NAN;
  std::string suffix = ToLower(Trim(std::string(end)));
  if (suffix.empty() || suffix == "px") return num;
  return NAN;
}

// Resolves the horizontal (`horizontal=true`) or vertical component of a clip-shape position token
// (`left/right/top/bottom/center` keyword or a `<length-percentage>`) into a pixel coordinate in
// the masked box's local space.
float ResolveClipPositionAxis(const std::string& token, float axis, bool horizontal) {
  std::string t = ToLower(Trim(token));
  if (t == "center") return axis / 2.0f;
  if (horizontal) {
    if (t == "left") return 0.0f;
    if (t == "right") return axis;
  } else {
    if (t == "top") return 0.0f;
    if (t == "bottom") return axis;
  }
  return ResolveClipCoord(token, axis);
}

// Splits `fn(args)` into a lowercased function name and the raw argument string between the
// outermost parentheses. Returns false when `value` is not a function form.
bool SplitCssFunction(const std::string& value, std::string& outName, std::string& outArgs) {
  std::string t = Trim(value);
  auto open = t.find('(');
  auto close = t.rfind(')');
  if (open == std::string::npos || close == std::string::npos || close <= open) return false;
  outName = ToLower(Trim(t.substr(0, open)));
  outArgs = Trim(t.substr(open + 1, close - open - 1));
  return true;
}

// Resolves one ellipse radius against its box axis. CSS permits the closest-side / farthest-side
// keywords independently on each axis; an omitted radius defaults to closest-side.
float ResolveEllipseRadius(const std::string& token, float axis, float center) {
  std::string value = ToLower(Trim(token));
  if (value.empty() || value == "closest-side") return std::min(center, axis - center);
  if (value == "farthest-side") return std::max(center, axis - center);
  return ResolveClipCoord(token, axis);
}

// Converts a CSS `clip-path` basic shape into SVG geometry expressed in the masked box's local
// pixel space (0..w, 0..h). The emitted shape(s) are painted opaque white by the framing <svg> and
// parsed through SVGImporter into a contour mask, exactly like a resolved `url(#id)` <clipPath>.
// Supports `polygon()`, `path()`, `circle()`, `ellipse()` and `inset()`; returns empty when the
// shape is malformed or uses an out-of-subset feature so the caller can drop it with a diagnostic.
std::string BuildClipShapeGeometry(const std::string& value, float w, float h) {
  std::string name;
  std::string args;
  if (!SplitCssFunction(value, name, args)) return {};

  if (name == "polygon") {
    auto items = SplitTopLevelCommas(args);
    if (items.empty()) return {};
    // An optional leading `<fill-rule>` (`nonzero` | `evenodd`) is its own comma-separated token.
    std::string fillRule;
    size_t start = 0;
    std::string firstLc = ToLower(Trim(items[0]));
    if (firstLc == "evenodd" || firstLc == "nonzero") {
      if (firstLc == "evenodd") fillRule = "evenodd";
      start = 1;
    }
    std::string points;
    for (size_t i = start; i < items.size(); ++i) {
      auto xy = SplitTopLevelWhitespace(items[i]);
      if (xy.size() < 2) return {};
      float x = ResolveClipCoord(xy[0], w);
      float y = ResolveClipCoord(xy[1], h);
      if (std::isnan(x) || std::isnan(y)) return {};
      if (!points.empty()) points += ' ';
      points += CssFloatToString(x) + "," + CssFloatToString(y);
    }
    if (points.empty()) return {};
    std::string el = "<polygon points=\"" + points + "\"";
    if (!fillRule.empty()) el += " fill-rule=\"" + fillRule + "\"";
    el += "/>";
    return el;
  }

  if (name == "path") {
    // path( [<fill-rule>,]? <string> ). A `<fill-rule>` prefix is only present when the value does
    // not start with the quoted path string (whose `d` data itself contains commas).
    std::string rest = Trim(args);
    if (!rest.empty() && rest.front() != '"' && rest.front() != '\'') {
      auto comma = rest.find(',');
      if (comma != std::string::npos) rest = Trim(rest.substr(comma + 1));
    }
    if (rest.size() >= 2 && (rest.front() == '"' || rest.front() == '\'') &&
        rest.back() == rest.front()) {
      rest = rest.substr(1, rest.size() - 2);
    }
    if (rest.empty()) return {};
    return "<path d=\"" + rest + "\"/>";
  }

  if (name == "circle") {
    // circle( [<radius>]? [at <position>]? )
    auto tokens = SplitTopLevelWhitespace(args);
    std::string radiusTok;
    std::vector<std::string> posTokens;
    size_t i = 0;
    if (i < tokens.size() && ToLower(tokens[i]) != "at") {
      radiusTok = tokens[i];
      ++i;
    }
    if (i < tokens.size() && ToLower(tokens[i]) == "at") {
      for (++i; i < tokens.size(); ++i) posTokens.push_back(tokens[i]);
    }
    float cx = posTokens.size() >= 1 ? ResolveClipPositionAxis(posTokens[0], w, true) : w / 2.0f;
    float cy = posTokens.size() >= 2 ? ResolveClipPositionAxis(posTokens[1], h, false) : h / 2.0f;
    if (std::isnan(cx) || std::isnan(cy)) return {};
    float r = NAN;
    std::string rl = ToLower(Trim(radiusTok));
    if (rl.empty() || rl == "closest-side") {
      r = std::min(std::min(cx, w - cx), std::min(cy, h - cy));
    } else if (rl == "farthest-side") {
      r = std::max(std::max(cx, w - cx), std::max(cy, h - cy));
    } else {
      float fraction = 0;
      // A percentage radius resolves against `sqrt(w^2 + h^2) / sqrt(2)` per the CSS shapes spec.
      if (ParseCssPercentage(radiusTok, fraction)) {
        r = fraction * std::sqrt(w * w + h * h) / std::sqrt(2.0f);
      } else {
        r = ResolveClipCoord(radiusTok, w);
      }
    }
    if (std::isnan(r) || r <= 0) return {};
    return "<circle cx=\"" + CssFloatToString(cx) + "\" cy=\"" + CssFloatToString(cy) + "\" r=\"" +
           CssFloatToString(r) + "\"/>";
  }

  if (name == "ellipse") {
    // ellipse( [<rx> <ry>]? [at <position>]? )
    auto tokens = SplitTopLevelWhitespace(args);
    std::vector<std::string> radii;
    std::vector<std::string> posTokens;
    size_t i = 0;
    for (; i < tokens.size() && ToLower(tokens[i]) != "at"; ++i) radii.push_back(tokens[i]);
    if (i < tokens.size() && ToLower(tokens[i]) == "at") {
      for (++i; i < tokens.size(); ++i) posTokens.push_back(tokens[i]);
    }
    float cx = posTokens.size() >= 1 ? ResolveClipPositionAxis(posTokens[0], w, true) : w / 2.0f;
    float cy = posTokens.size() >= 2 ? ResolveClipPositionAxis(posTokens[1], h, false) : h / 2.0f;
    if (std::isnan(cx) || std::isnan(cy)) return {};
    float rx = ResolveEllipseRadius(radii.size() > 0 ? radii[0] : "", w, cx);
    float ry = ResolveEllipseRadius(radii.size() > 1 ? radii[1] : "", h, cy);
    if (std::isnan(rx) || std::isnan(ry) || rx <= 0 || ry <= 0) return {};
    return "<ellipse cx=\"" + CssFloatToString(cx) + "\" cy=\"" + CssFloatToString(cy) +
           "\" rx=\"" + CssFloatToString(rx) + "\" ry=\"" + CssFloatToString(ry) + "\"/>";
  }

  if (name == "inset") {
    // inset( <top> <right>? <bottom>? <left>? [round <border-radius>]? )
    auto tokens = SplitTopLevelWhitespace(args);
    std::vector<std::string> offs;
    std::vector<std::string> roundToks;
    bool afterRound = false;
    for (auto& tk : tokens) {
      if (!afterRound && ToLower(tk) == "round") {
        afterRound = true;
        continue;
      }
      (afterRound ? roundToks : offs).push_back(tk);
    }
    if (offs.empty() || offs.size() > 4) return {};
    // CSS edge shorthand: 1 -> all, 2 -> (top/bottom, left/right), 3 -> (top, left/right, bottom),
    // 4 -> (top, right, bottom, left).
    std::string tS = offs[0];
    std::string rS = offs.size() >= 2 ? offs[1] : offs[0];
    std::string bS = offs.size() >= 3 ? offs[2] : tS;
    std::string lS = offs.size() >= 4 ? offs[3] : rS;
    float top = ResolveClipCoord(tS, h);
    float right = ResolveClipCoord(rS, w);
    float bottom = ResolveClipCoord(bS, h);
    float left = ResolveClipCoord(lS, w);
    if (std::isnan(top) || std::isnan(right) || std::isnan(bottom) || std::isnan(left)) return {};
    float rw = w - left - right;
    float rh = h - top - bottom;
    if (rw <= 0 || rh <= 0) return {};
    std::string rect = "<rect x=\"" + CssFloatToString(left) + "\" y=\"" + CssFloatToString(top) +
                       "\" width=\"" + CssFloatToString(rw) + "\" height=\"" +
                       CssFloatToString(rh) + "\"";
    // Only the uniform `round <r>` form is modelled; the elliptical / per-corner border-radius
    // grammar collapses to the first radius token applied to both axes.
    if (!roundToks.empty()) {
      float rr = ResolveClipCoord(roundToks[0], w);
      if (!std::isnan(rr) && rr > 0) rect += " rx=\"" + CssFloatToString(rr) + "\"";
    }
    rect += "/>";
    return rect;
  }

  return {};
}

}  // namespace

bool HTMLParserContext::foldRoundedImageWrapper(const std::shared_ptr<DOMNode>& element,
                                                const HTMLBoxAttributes& box, Layer* layer) {
  if (!box.borderRadiusSet || !box.clipOverflow) return false;
  if (HTMLLayerBuilder::requiresInnerHost(box)) return false;

  // Walk down through up to `HTML_ROUND_IMAGE_MAX_WRAPPER_DEPTH` intermediate
  // layout-only `<div>` wrappers that share `element`'s content box. The
  // html-snapshot pipeline emits one such wrapper per `<img>` (see
  // `renderBoxedReplaced` in `tools/html-snapshot/lib/browser-snapshot.ts`)
  // for border-overlay routing, and authored HTML occasionally adds another for
  // positioning / aspect-ratio. Without this loop the fold would only match the
  // wrapper-direct-on-img layout and render those nested cases as a square box
  // through the outer container's transparent rounded layer.
  std::shared_ptr<DOMNode> img = FindSoleElementChild(element);
  for (int depth = 0; depth < HTML_ROUND_IMAGE_MAX_WRAPPER_DEPTH; ++depth) {
    if (!img) return false;
    if (img->name == "img") break;
    if (img->name != "div") return false;
    HTMLBoxAttributes wrapperBox = _styleCascade->computeBoxAttributes(img);
    if (!IsLayoutOnlyWrapperBox(wrapperBox)) return false;
    if (!WrapperBoxCoversOuter(wrapperBox, box)) return false;
    img = FindSoleElementChild(img);
  }
  if (!img || img->name != "img") return false;

  // Reject SVG sources up front: they ride an import directive, not a raster fill.
  auto* srcAttr = img->findAttribute("src");
  if (!srcAttr || srcAttr->empty()) return false;
  const std::string& src = *srcAttr;
  if (IsExternalSvgSrc(src)) return false;

  // The image must exactly cover the outer wrapper's content box, anchored at
  // top-left — otherwise the rounded clip would shape only part of the visible
  // image and folding would stretch it across the wrapper.
  HTMLBoxAttributes imgBox = _styleCascade->computeBoxAttributes(img);
  if (!WrapperBoxCoversOuter(imgBox, box)) return false;

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

  // The fold collapses the wrapper + inner `<img>` into a single layer. The wrapper's own
  // `data-*` were already forwarded when its layer attributes were applied; forward the inner
  // `<img>`'s too so image-carried custom data (e.g. `data-id`) is not lost to the fold.
  _layerBuilder->forwardDataAttributes(layer, img);

  // Use assignElementId (not a bare _idAllocator->assign) so an `animation` style on the folded
  // element is still recorded for later PAGX animation emission.
  assignElementId(layer, element);
  return true;
}

Layer* HTMLParserContext::convertImage(const std::shared_ptr<DOMNode>& element,
                                       const HTMLBoxAttributes& box) {
  auto* srcAttr = element->findAttribute("src");
  const std::string src = (srcAttr != nullptr) ? *srcAttr : std::string();
  if (IsExternalSvgSrc(src)) {
    auto layer = _document->makeNode<Layer>();
    _layerBuilder->applySizeAndPosition(layer, box);
    _layerBuilder->applyLayerAttributes(layer, element, box);
    layer->importDirective.source = resolveImageSource(src);
    layer->importDirective.format = "svg";
    assignElementId(layer, element);
    return layer;
  }

  // A missing / empty `src` is not a reason to drop the element: the `<img>` still occupies a
  // box in the layout and carries its own visuals (size, border-radius, data-*). Preserve the
  // layer and back its fill with a placeholder `Image` whose source is left unresolved, so the
  // image slot survives the import instead of vanishing from the PAGX. A non-empty `src` always
  // registers an `Image` (even a broken path decodes to an unloadable-but-present resource).
  Image* imageNode = src.empty() ? nullptr : registerImageResource(resolveImageSource(src));
  if (imageNode == nullptr) {
    warn("html: <img> missing src; image preserved with unresolved source");
    imageNode = _imageResources->createPlaceholder();
  }

  auto layer = _document->makeNode<Layer>();
  _layerBuilder->applySizeAndPosition(layer, box);
  _layerBuilder->applyLayerAttributes(layer, element, box);

  // Honour `border-radius` directly on `<img>`: a CSS `<img style="border-radius: 50%">` is the
  // canonical way to draw a circular avatar, and per-corner radii (e.g. only the top corners
  // rounded for a "card cover" image) follow the same Path-emission path the container code
  // uses. When the image has no border-radius, `buildBackgroundGeometry` falls back to a plain
  // `Rectangle width=100% height=100%`, preserving the legacy emission verbatim.
  layer->contents.push_back(_layerBuilder->buildBackgroundGeometry(box));

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
  _layerBuilder->applySizeAndPosition(layer, box);
  _layerBuilder->applyLayerAttributes(layer, element, box);

  // CSS `color` cascades into the SVG and is what `currentColor` resolves to.
  // `resolveInheritedStyle` returns the style descendants see, but `resolvedTextColor`
  // is the *element's own* colour after applying any `style="color: …"` on the
  // SVG itself — exactly what `currentColor` should resolve to at the SVG root.
  HTMLInheritedStyle svgStyle = _styleCascade->resolveInheritedStyle(element, inherited);
  std::string rootColor = HTMLInlineSvgEmitter::formatColorForAttribute(svgStyle.resolvedTextColor);
  _svgEmitter->resolveCurrentColor(element, rootColor);

  // Rewrite the exporter's conic-gradient-stroke workaround (`<mask>` + `<foreignObject>` with a
  // CSS-gradient `<div>`) back into native stroked shapes. The downstream SVG importer does not
  // understand `<foreignObject>` and would otherwise paint the masked region opaque black, dropping
  // the shape. Run before defs injection so the cleaned tree drives the remaining passes.
  _svgEmitter->reconstructForeignObjectPaints(element, *_valueParser);

  // Pull in any shared `<defs>` (gradients/patterns/clip paths) this SVG references by id but
  // does not itself define, so the standalone import directive resolves `url(#…)` locally.
  _svgEmitter->injectReferencedDefs(element);

  // `applyLayerAttributes` already hoisted the SVG element's CSS `opacity` onto `layer->alpha`.
  // Strip the root `<svg>`'s own opacity before serialising so the nested SVG importer does not
  // apply it a second time (which would multiply the two and wash the shape out).
  if (box.opacitySet) {
    _svgEmitter->stripRootOpacity(element);
  }

  // Record animated shape descendants before serialisation so any minted `id` is baked into the
  // import directive and the SVG importer can derive matching Fill / Stroke painter ids.
  collectInlineSvgShapeAnimations(element);

  layer->importDirective.content = _svgEmitter->serialize(element);
  layer->importDirective.format = "svg";
  assignElementId(layer, element);
  return layer;
}

void HTMLParserContext::collectInlineSvgShapeAnimations(const std::shared_ptr<DOMNode>& node) {
  if (node == nullptr) {
    return;
  }
  for (auto child = node->getFirstChild(); child; child = child->getNextSibling()) {
    if (child->type != DOMNodeType::Element) {
      continue;
    }
    const std::string* styleAttr = child->findAttribute("style");
    if (styleAttr != nullptr) {
      std::unordered_map<std::string, std::string> style;
      ParseStyleString(*styleAttr, style);
      if (style.count("animation") > 0 || style.count("animation-name") > 0) {
        // A stable id must survive serialisation so the SVG importer can key the derived painter
        // ids off it. Reuse an authored id; otherwise mint a fresh non-colliding one.
        std::string id;
        const std::string* idAttr = child->findAttribute("id");
        if (idAttr != nullptr && !idAttr->empty()) {
          id = *idAttr;
        } else {
          id = _idAllocator->generateUnique("svgshape");
          bool replaced = false;
          for (auto& attr : child->attributes) {
            if (attr.name == "id") {
              attr.value = id;
              replaced = true;
              break;
            }
          }
          if (!replaced) {
            child->attributes.push_back(DOMAttribute{std::string("id"), id});
          }
        }

        // `stroke-dashoffset` keyframes are captured in author space (normalised by `pathLength`);
        // resolve the real-to-author length ratio here where the geometry `d` is available, matching
        // the static dash scaling the SVG importer applies (SVGParserContext::computePathTotalLength).
        float dashScale = 1.0f;
        const std::string* pathLen = child->findAttribute("pathLength");
        const std::string* dAttr = child->findAttribute("d");
        if (pathLen != nullptr && dAttr != nullptr && child->name == "path") {
          float authorLength = std::strtof(pathLen->c_str(), nullptr);
          if (authorLength > 0.0f) {
            tgfx::Path path = ToTGFX(PathDataFromSVGString(*dAttr));
            if (!path.isEmpty()) {
              auto measure = tgfx::PathMeasure::MakeFrom(path);
              if (measure != nullptr) {
                float total = 0.0f;
                do {
                  total += measure->getLength();
                } while (measure->nextContour());
                if (total > 0.0f) {
                  dashScale = total / authorLength;
                }
              }
            }
          }
        }

        PendingSvgShapeAnimation pending;
        pending.style = std::move(style);
        pending.shapeTargetId = id;
        pending.fillTargetId = id + "__fill";
        pending.strokeTargetId = id + "__stroke";
        pending.dashScale = dashScale;
        _pendingSvgShapeAnimations.push_back(std::move(pending));
      }
    }
    collectInlineSvgShapeAnimations(child);
  }
}

std::string HTMLParserContext::serializeSvg(const std::shared_ptr<DOMNode>& svgNode) {
  return _svgEmitter->serialize(svgNode);
}

void HTMLParserContext::applyMaskOrClip(Layer* layer, const HTMLBoxAttributes& box) {
  if (layer == nullptr) return;
  bool hasMaskImage = !box.maskImage.empty() && ToLower(box.maskImage).find("none") != 0;
  bool hasClipPath = !box.clipPathRef.empty();
  if (!hasMaskImage && !hasClipPath) return;
  // A mask and a clip-path on the same element is not something the exporter emits and tgfx
  // models only a single mask per layer; honour the mask-image (alpha/luminance) and warn that
  // the clip-path is dropped, matching the "one mask wins" rule.
  if (hasMaskImage && hasClipPath) {
    warn("html: element carries both mask-image and clip-path; clip-path ignored");
    hasClipPath = false;
  }

  std::string svgContent;
  MaskType maskType = MaskType::Alpha;
  if (hasMaskImage) {
    std::string url = ExtractCssUrl(box.maskImage);
    svgContent = DecodeSvgDataUri(url);
    if (svgContent.empty()) {
      // Not an SVG data URI. A raster `mask-image` (PNG / JPEG / WebP via a file path, `http(s)`
      // URL, or `data:image/<raster>` URI) is masked through an image-backed layer instead of the
      // SVG geometry path below. Fall back to warning and dropping only when that also fails.
      if (applyRasterImageMask(layer, box, url)) {
        return;
      }
      warn(
          "html: mask-image is not a decodable data:image/svg+xml URI or loadable raster image; "
          "ignored");
      return;
    }
    maskType = (box.maskMode == "luminance") ? MaskType::Luminance : MaskType::Alpha;
  } else {
    // CSS `clip-path`: either a `url(#id)` reference to a hidden <clipPath> def, or a basic shape
    // (`polygon()/path()/circle()/ellipse()/inset()`). Both are rebuilt into a contour mask framed
    // to the masked box's local coordinate space; the clip shapes are painted opaque white because
    // contour masking only reads their coverage, not their colour.
    float w = std::isnan(box.widthPx) ? _canvasWidth : box.widthPx;
    float h = std::isnan(box.heightPx) ? _canvasHeight : box.heightPx;
    std::string trimmedClip = Trim(box.clipPathRef);
    bool isUrl = ToLower(trimmedClip).compare(0, 4, "url(") == 0;
    std::string inner;
    // The framing viewBox maps geometry coordinates onto the pixel box. Basic shapes and
    // `userSpaceOnUse` clipPaths carry pixel coordinates (0..w / 0..h); `objectBoundingBox`
    // clipPaths carry 0..1 fractions, mapped by a unit-square viewBox at the box's pixel size.
    std::string viewBox = "0 0 " + CssFloatToString(w) + " " + CssFloatToString(h);
    if (isUrl) {
      std::string id = ExtractCssUrl(box.clipPathRef);
      if (!id.empty() && id.front() == '#') {
        id = id.substr(1);
      }
      auto clipNode = id.empty() ? nullptr : _svgEmitter->lookupSharedDef(id);
      if (clipNode == nullptr) {
        warn("html: clip-path references an unknown <clipPath> id; ignored");
        return;
      }
      for (auto child = clipNode->getFirstChild(); child; child = child->getNextSibling()) {
        inner += _svgEmitter->serialize(child);
      }
      if (inner.empty()) {
        warn("html: clip-path <clipPath> has no geometry; ignored");
        return;
      }
      std::string units = ToLower(Trim(GetDomAttribute(clipNode, "clippathunits")));
      if (units == "objectboundingbox") {
        viewBox = "0 0 1 1";
      }
    } else {
      inner = BuildClipShapeGeometry(trimmedClip, w, h);
      if (inner.empty()) {
        warn("html: unsupported clip-path basic shape; ignored");
        return;
      }
    }
    svgContent = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" + CssFloatToString(w) +
                 "\" height=\"" + CssFloatToString(h) + "\" viewBox=\"" + viewBox +
                 "\" fill=\"white\">" + inner + "</svg>";
    maskType = MaskType::Contour;
  }

  auto svgDoc = SVGImporter::ParseString(svgContent);
  if (svgDoc == nullptr || svgDoc->layers.empty()) {
    warn("html: failed to rebuild mask geometry from SVG; ignored");
    return;
  }
  // The SVG was imported at its intrinsic pixel size (viewBox mapped into the SVG's own
  // width/height). CSS `mask-size` / `mask-position` then scale and offset that intrinsic box onto
  // the masked element, so capture the intrinsic dimensions before the document is drained below.
  float intrinsicW = svgDoc->width;
  float intrinsicH = svgDoc->height;

  // The mask SVG produces one or more content layers (a single drawn shape in the common case).
  // Wrap them under one invisible, layout-excluded mask layer so a multi-shape clip-path also
  // works, then transplant every node from the temporary SVG document into ours.
  Layer* maskLayer = nullptr;
  if (svgDoc->layers.size() == 1) {
    maskLayer = svgDoc->layers[0];
  } else {
    maskLayer = _document->makeNode<Layer>();
    for (auto* l : svgDoc->layers) {
      maskLayer->children.push_back(l);
    }
  }
  maskLayer->visible = false;
  maskLayer->includeInLayout = false;
  // CSS `mask-size` / `mask-position` scale and offset the intrinsic mask box onto the masked
  // element; replay that transform onto the mask layer. Contour clip-paths are framed to the box
  // directly above and carry no size/position, so this only applies to mask-image masks.
  if (hasMaskImage) {
    applyMaskSizeAndPosition(maskLayer, box, intrinsicW, intrinsicH);
  }
  // The mask layer needs a stable id so the exporter can serialise the `mask="@id"` reference;
  // without one the masked layer would lose its mask on a PAGX round-trip (export then re-import).
  if (maskLayer->id.empty()) {
    maskLayer->id = _idAllocator->generateUnique("mask");
  }
  for (auto& node : svgDoc->nodes) {
    _document->nodes.push_back(std::move(node));
  }
  svgDoc->nodes.clear();
  svgDoc->layers.clear();

  layer->mask = maskLayer;
  layer->maskType = maskType;
  // The mask layer must be reachable in the display list for the renderer's mask lookup, and must
  // share the masked layer's local coordinate origin. Adding it as an invisible, layout-excluded
  // child satisfies both: it is walked by LayerBuilder but neither drawn (maskOwner is set) nor
  // laid out (includeInLayout is false).
  layer->children.push_back(maskLayer);
}

bool HTMLParserContext::applyRasterImageMask(Layer* layer, const HTMLBoxAttributes& box,
                                             const std::string& url) {
  if (url.empty()) return false;
  auto* imageNode = registerImageResource(resolveImageSource(url));
  if (imageNode == nullptr) return false;
  // The mask is scaled / positioned relative to its intrinsic pixel box (the CSS `mask-size` /
  // `mask-position` model), so the native dimensions must be decodable up front. A zero size means
  // the bytes could not be read — let the caller warn and drop rather than emit a degenerate mask.
  auto nativeSize = decodeImageNativeSize(imageNode);
  if (nativeSize.first <= 0 || nativeSize.second <= 0) return false;
  float intrinsicW = static_cast<float>(nativeSize.first);
  float intrinsicH = static_cast<float>(nativeSize.second);

  // Build the mask layer as a native-sized rectangle filled with the image, anchored at the origin
  // — the same coordinate convention the SVG-mask path relies on. `ScaleMode::Stretch` paints the
  // image across the full rectangle box; since the rectangle is the image's native size the paint
  // is 1:1, and `applyMaskSizeAndPosition` below then scales / offsets the whole layer to match CSS.
  auto* maskLayer = _document->makeNode<Layer>();
  auto* rect = _document->makeNode<Rectangle>();
  rect->size = {intrinsicW, intrinsicH};
  maskLayer->contents.push_back(rect);
  auto* fill = _document->makeNode<Fill>();
  auto* pattern = _document->makeNode<ImagePattern>();
  pattern->image = imageNode;
  pattern->scaleMode = ScaleMode::Stretch;
  fill->color = pattern;
  maskLayer->contents.push_back(fill);

  maskLayer->visible = false;
  maskLayer->includeInLayout = false;
  applyMaskSizeAndPosition(maskLayer, box, intrinsicW, intrinsicH);
  // A stable id keeps the `mask="@id"` reference intact across a PAGX export / re-import round-trip.
  if (maskLayer->id.empty()) {
    maskLayer->id = _idAllocator->generateUnique("mask");
  }

  layer->mask = maskLayer;
  // A raster `mask-image` defaults to `mask-mode: match-source`, which for an image source reads its
  // alpha channel; `mask-mode: luminance` opts into the luma-keyed form instead.
  layer->maskType = (box.maskMode == "luminance") ? MaskType::Luminance : MaskType::Alpha;
  layer->children.push_back(maskLayer);
  return true;
}

float HTMLParserContext::resolveMaskPositionAxis(const std::string& token, float boxAxis,
                                                 float maskAxis) {
  if (token.empty() || token == "left" || token == "top") {
    return 0.0f;
  }
  if (token == "right" || token == "bottom") {
    return boxAxis - maskAxis;
  }
  if (token == "center") {
    return (boxAxis - maskAxis) / 2.0f;
  }
  float fraction = 0;
  if (ParseCssPercentage(token, fraction)) {
    return fraction * (boxAxis - maskAxis);
  }
  float px = _valueParser->parseAbsoluteLengthPx(token);
  return std::isnan(px) ? 0.0f : px;
}

void HTMLParserContext::applyMaskSizeAndPosition(Layer* maskLayer, const HTMLBoxAttributes& box,
                                                 float intrinsicW, float intrinsicH) {
  if (maskLayer == nullptr || !(intrinsicW > 0) || !(intrinsicH > 0)) return;
  float boxW = std::isnan(box.widthPx) ? _canvasWidth : box.widthPx;
  float boxH = std::isnan(box.heightPx) ? _canvasHeight : box.heightPx;

  // Resolve `mask-size` into the on-element pixel box, then divide by the intrinsic box to recover
  // the per-axis scale. The single-value, `auto`, `contain` and `cover` forms follow the CSS
  // background/mask sizing model; the exporter's own output is the two-length form.
  float scaleX = 1.0f;
  float scaleY = 1.0f;
  auto sizeTokens = SplitTopLevelWhitespace(box.maskSize);
  std::string sizeW = sizeTokens.size() > 0 ? sizeTokens[0] : "";
  std::string sizeH = sizeTokens.size() > 1 ? sizeTokens[1] : "";
  if (sizeW == "contain" || sizeW == "cover") {
    float fitX = boxW / intrinsicW;
    float fitY = boxH / intrinsicH;
    float fit = (sizeW == "contain") ? std::min(fitX, fitY) : std::max(fitX, fitY);
    scaleX = fit;
    scaleY = fit;
  } else if (!sizeW.empty()) {
    // Per-axis target length: `auto` (NaN here) keeps the axis tied to the other so the aspect
    // ratio is preserved, matching CSS when only one dimension is given.
    float targetW = ResolveMaskSizeAxis(sizeW, boxW, intrinsicW, *_valueParser);
    float targetH = ResolveMaskSizeAxis(sizeH, boxH, intrinsicH, *_valueParser);
    if (std::isnan(targetW) && std::isnan(targetH)) {
      // both auto -> intrinsic size, scale 1
    } else if (std::isnan(targetW)) {
      scaleY = targetH / intrinsicH;
      scaleX = scaleY;
    } else if (std::isnan(targetH)) {
      scaleX = targetW / intrinsicW;
      scaleY = scaleX;
    } else {
      scaleX = targetW / intrinsicW;
      scaleY = targetH / intrinsicH;
    }
  }

  // The scaled mask box used to resolve percentage / keyword `mask-position` against the element.
  float scaledW = intrinsicW * scaleX;
  float scaledH = intrinsicH * scaleY;
  auto posTokens = SplitTopLevelWhitespace(box.maskPosition);
  std::string posX = posTokens.size() > 0 ? posTokens[0] : "";
  // CSS resolves a single `mask-position` value as the horizontal axis with the vertical defaulting
  // to `center`, not to the leading edge. Two empty tokens keep the `0 0` top-left default below.
  std::string posY = posTokens.size() > 1 ? posTokens[1] : posTokens.size() == 1 ? "center" : "";
  float tx = resolveMaskPositionAxis(posX, boxW, scaledW);
  float ty = resolveMaskPositionAxis(posY, boxH, scaledH);

  // Geometry sits in the intrinsic box anchored at the origin; scale about (0,0) then translate so
  // the mask lands where CSS positions it. Compose ahead of any transform the SVG import produced.
  Matrix transform = Matrix::Translate(tx, ty) * Matrix::Scale(scaleX, scaleY);
  maskLayer->matrix = transform * maskLayer->matrix;
}

bool HTMLParserContext::applyBackgroundImageFill(const HTMLBoxAttributes& box, Layer* layer) {
  std::string src = ExtractCssUrl(box.backgroundImage);
  if (src.empty()) return false;
  auto* imageNode = registerImageResource(resolveImageSource(src));
  if (!imageNode) return false;

  auto pattern = _document->makeNode<ImagePattern>();
  pattern->image = imageNode;

  // `background-size` keywords map onto a fitted scaleMode that centres the image and ignores
  // repeat/position — the inverse of the exporter's letterbox/zoom/stretch emission. Anything
  // else (an explicit pixel size, or a tiling repeat) becomes ScaleMode::None with the matrix
  // carrying the per-axis scale and the position offset.
  const std::string& size = box.backgroundSize;
  bool fitted = false;
  if (size == "contain") {
    pattern->scaleMode = ScaleMode::LetterBox;
    fitted = true;
  } else if (size == "cover") {
    pattern->scaleMode = ScaleMode::Zoom;
    fitted = true;
  } else if (size == "100% 100%") {
    pattern->scaleMode = ScaleMode::Stretch;
    fitted = true;
  }

  if (!fitted) {
    pattern->scaleMode = ScaleMode::None;
    ResolveBackgroundRepeat(box.backgroundRepeat, pattern->tileModeX, pattern->tileModeY);

    // Per-axis scale: the on-screen tile size (`background-size: <w>px <h>px`) divided by the
    // image's native pixel size mirrors the exporter's `tileW = sx * imgW` emission. Without an
    // explicit size the tile is the image's native size, i.e. scale 1.
    auto sizeTokens = SplitTopLevelWhitespace(size);
    float tileW = sizeTokens.size() > 0 ? _valueParser->parseAbsoluteLengthPx(sizeTokens[0]) : NAN;
    float tileH =
        sizeTokens.size() > 1 ? _valueParser->parseAbsoluteLengthPx(sizeTokens[1]) : tileW;
    auto nativeSize = decodeImageNativeSize(imageNode);
    if (!std::isnan(tileW) && tileW > 0 && nativeSize.first > 0) {
      pattern->matrix.a = tileW / static_cast<float>(nativeSize.first);
    }
    if (!std::isnan(tileH) && tileH > 0 && nativeSize.second > 0) {
      pattern->matrix.d = tileH / static_cast<float>(nativeSize.second);
    }

    // `background-position: <x>px <y>px` is the tile origin relative to this element's own box.
    // Each re-imported card is a standalone Layer whose contents share the box origin, so the
    // position maps straight onto the pattern matrix translation (the exporter writes the same
    // value back, offset by the layer's own left/top which is zero in the standalone case).
    auto posTokens = SplitTopLevelWhitespace(box.backgroundPosition);
    if (posTokens.size() > 0) {
      float posX = _valueParser->parseAbsoluteLengthPx(posTokens[0]);
      if (!std::isnan(posX)) pattern->matrix.tx = posX;
    }
    if (posTokens.size() > 1) {
      float posY = _valueParser->parseAbsoluteLengthPx(posTokens[1]);
      if (!std::isnan(posY)) pattern->matrix.ty = posY;
    }
  }

  auto fill = _document->makeNode<Fill>();
  fill->color = pattern;
  // A `background-blend-mode` blends this url() image against the background-color that
  // applyBackgroundFill emits underneath it, mirroring the gradient path. With no colour the
  // image is the bottom-most background layer and has nothing to blend against, so it draws
  // Normal (matching CSS, where a lone layer's blend mode is a no-op).
  if (box.backgroundColorSet) {
    fill->blendMode = HTMLLayerBuilder::resolveBackgroundBlendMode(box.backgroundBlendMode);
  }
  layer->contents.push_back(fill);
  return true;
}

std::pair<int, int> HTMLParserContext::decodeImageNativeSize(const Image* image) {
  if (!image) return {0, 0};
  std::shared_ptr<tgfx::ImageCodec> codec;
  if (image->data && image->data->size() > 0) {
    auto data = tgfx::Data::MakeWithCopy(image->data->bytes(), image->data->size());
    codec = tgfx::ImageCodec::MakeFrom(std::move(data));
  } else if (!image->filePath.empty()) {
    if (image->filePath.compare(0, 5, "data:") == 0) {
      auto decoded = DecodeBase64DataURI(image->filePath);
      if (decoded && decoded->size() > 0) {
        auto data = tgfx::Data::MakeWithCopy(decoded->bytes(), decoded->size());
        codec = tgfx::ImageCodec::MakeFrom(std::move(data));
      }
    } else {
      codec = tgfx::ImageCodec::MakeFrom(image->filePath);
    }
  }
  if (!codec) return {0, 0};
  return {codec->width(), codec->height()};
}

Image* HTMLParserContext::registerImageResource(const std::string& imageSource) {
  return _imageResources->registerResource(imageSource);
}

std::string HTMLParserContext::resolveImageSource(const std::string& src) const {
  return _imageResources->resolveSource(src);
}

}  // namespace pagx
