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

#include "pagx/html/importer/HTMLStyleCascade.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
#include "base/utils/MathUtil.h"
#include "pagx/html/importer/HTMLCssCascade.h"
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLDiagnosticSink.h"
#include "pagx/html/importer/HTMLValueParser.h"
#include "pagx/utils/CSSFontStyle.h"
#include "pagx/xml/XMLDOM.h"

namespace pagx {

using namespace pagx::html;

namespace {

// Splits a CSS function argument list (the slice between `(` and `)`) on top-level commas,
// trimming each token. Used by `ParseTransformFunction` so multi-arg forms like
// `scale(1.5, 0.75)` and `translate(10px, 20px)` round-trip without a heavier parser.
std::vector<std::string> SplitCommaArgs(const std::string& args) {
  std::vector<std::string> out;
  size_t start = 0;
  for (size_t i = 0; i <= args.size(); ++i) {
    if (i == args.size() || args[i] == ',') {
      out.push_back(Trim(args.substr(start, i - start)));
      start = i + 1;
    }
  }
  return out;
}

// Pulls the function name and the argument-list slice from "fn(args)" form. The CSS subset
// transformer in HTMLSubsetPropertyTable already rejects compound chains and `matrix(...)`
// before we see the value, but the resolver runs even when subset transformation is bypassed
// (e.g. `HTMLImporter::Options::autoNormalize == false`), so we re-check the shape here via
// `ParseCssFunctionCall`: the FIRST `)` must coincide with the LAST `)` and trail only
// whitespace, otherwise the value is a compound chain and is rejected.

bool ParseScalarFloat(const std::string& token, float& out) {
  std::string trimmed = Trim(token);
  if (trimmed.empty()) return false;
  char* end = nullptr;
  float v = std::strtof(trimmed.c_str(), &end);
  if (end == trimmed.c_str()) return false;
  if (!Trim(end).empty()) return false;
  out = v;
  return true;
}

// `parseBoxTransform` per-function handler context. The diagnostics sink and value parser
// are forwarded so the static handlers can stay outside the class while still emitting the
// same warnings the class member used to.
struct TransformParseCtx {
  const std::string& transform;
  const std::vector<std::string>& parts;
  HTMLDiagnosticSink& diagnostics;
  HTMLValueParser& valueParser;
};

// Each handler returns true (and fills `parsed`) on success; false means a diagnostic was
// already emitted and the caller should bail.
using TransformHandler = bool (*)(const TransformParseCtx&, HTMLTransform&);

bool ParseSkewX(const TransformParseCtx& ctx, HTMLTransform& parsed) {
  if (ctx.parts.size() != 1) {
    ctx.diagnostics.warn("html: skewX expects 1 argument; got '" + ctx.transform + "'");
    return false;
  }
  parsed.skew = -ParseAngle(ctx.parts[0]);
  parsed.skewAxis = 0.0f;
  parsed.valid = true;
  return true;
}

bool ParseSkewY(const TransformParseCtx& ctx, HTMLTransform& parsed) {
  if (ctx.parts.size() != 1) {
    ctx.diagnostics.warn("html: skewY expects 1 argument; got '" + ctx.transform + "'");
    return false;
  }
  parsed.skew = ParseAngle(ctx.parts[0]);
  parsed.skewAxis = 90.0f;
  parsed.valid = true;
  return true;
}

bool ParseRotate(const TransformParseCtx& ctx, HTMLTransform& parsed) {
  if (ctx.parts.size() != 1) {
    ctx.diagnostics.warn("html: rotate expects 1 argument; got '" + ctx.transform + "'");
    return false;
  }
  parsed.rotation = ParseAngle(ctx.parts[0]);
  parsed.valid = true;
  return true;
}

bool ParseScale(const TransformParseCtx& ctx, HTMLTransform& parsed) {
  float sx = 1.0f;
  float sy = 1.0f;
  if (ctx.parts.size() == 1 && ParseScalarFloat(ctx.parts[0], sx)) {
    sy = sx;
  } else if (ctx.parts.size() == 2 && ParseScalarFloat(ctx.parts[0], sx) &&
             ParseScalarFloat(ctx.parts[1], sy)) {
    // sx/sy already filled.
  } else {
    ctx.diagnostics.warn("html: scale expects 1 or 2 numeric arguments; got '" + ctx.transform +
                         "'");
    return false;
  }
  parsed.scaleX = sx;
  parsed.scaleY = sy;
  parsed.valid = true;
  return true;
}

bool ParseScaleX(const TransformParseCtx& ctx, HTMLTransform& parsed) {
  float v = 1.0f;
  if (ctx.parts.size() != 1 || !ParseScalarFloat(ctx.parts[0], v)) {
    ctx.diagnostics.warn("html: scaleX expects 1 numeric argument; got '" + ctx.transform + "'");
    return false;
  }
  parsed.scaleX = v;
  parsed.valid = true;
  return true;
}

bool ParseScaleY(const TransformParseCtx& ctx, HTMLTransform& parsed) {
  float v = 1.0f;
  if (ctx.parts.size() != 1 || !ParseScalarFloat(ctx.parts[0], v)) {
    ctx.diagnostics.warn("html: scaleY expects 1 numeric argument; got '" + ctx.transform + "'");
    return false;
  }
  parsed.scaleY = v;
  parsed.valid = true;
  return true;
}

bool ParseTranslate(const TransformParseCtx& ctx, HTMLTransform& parsed) {
  if (ctx.parts.empty() || ctx.parts.size() > 2) {
    ctx.diagnostics.warn("html: translate expects 1 or 2 length arguments; got '" + ctx.transform +
                         "'");
    return false;
  }
  float tx = ctx.valueParser.parseAbsoluteLengthPx(ctx.parts[0]);
  if (std::isnan(tx)) {
    ctx.diagnostics.warn("html: translate first argument '" + ctx.parts[0] +
                         "' is not a px length; ignored");
    return false;
  }
  float ty = 0.0f;
  if (ctx.parts.size() == 2) {
    ty = ctx.valueParser.parseAbsoluteLengthPx(ctx.parts[1]);
    if (std::isnan(ty)) {
      ctx.diagnostics.warn("html: translate second argument '" + ctx.parts[1] +
                           "' is not a px length; ignored");
      return false;
    }
  }
  parsed.translateX = tx;
  parsed.translateY = ty;
  parsed.valid = true;
  return true;
}

bool ParseTranslateX(const TransformParseCtx& ctx, HTMLTransform& parsed) {
  if (ctx.parts.size() != 1) {
    ctx.diagnostics.warn("html: translateX expects 1 length argument; got '" + ctx.transform + "'");
    return false;
  }
  float v = ctx.valueParser.parseAbsoluteLengthPx(ctx.parts[0]);
  if (std::isnan(v)) {
    ctx.diagnostics.warn("html: translateX argument '" + ctx.parts[0] +
                         "' is not a px length; ignored");
    return false;
  }
  parsed.translateX = v;
  parsed.valid = true;
  return true;
}

bool ParseTranslateY(const TransformParseCtx& ctx, HTMLTransform& parsed) {
  if (ctx.parts.size() != 1) {
    ctx.diagnostics.warn("html: translateY expects 1 length argument; got '" + ctx.transform + "'");
    return false;
  }
  float v = ctx.valueParser.parseAbsoluteLengthPx(ctx.parts[0]);
  if (std::isnan(v)) {
    ctx.diagnostics.warn("html: translateY argument '" + ctx.parts[0] +
                         "' is not a px length; ignored");
    return false;
  }
  parsed.translateY = v;
  parsed.valid = true;
  return true;
}

// `matrix(a, b, c, d, tx, ty)` populates `parsed.matrix` directly (the discrete fields are
// ambiguous from a raw matrix). The caller skips the post-pass discrete-field composition for
// this form, so `parsed.valid` carries the identity check used by downstream consumers.
bool ParseMatrix(const TransformParseCtx& ctx, HTMLTransform& parsed) {
  if (ctx.parts.size() != 6) {
    ctx.diagnostics.warn("html: matrix expects 6 numeric arguments; got '" + ctx.transform + "'");
    return false;
  }
  float m[6];
  for (size_t i = 0; i < 6; i++) {
    if (!ParseScalarFloat(ctx.parts[i], m[i])) {
      ctx.diagnostics.warn("html: matrix argument '" + ctx.parts[i] + "' is not a number; ignored");
      return false;
    }
  }
  parsed.matrix.a = m[0];
  parsed.matrix.b = m[1];
  parsed.matrix.c = m[2];
  parsed.matrix.d = m[3];
  parsed.matrix.tx = m[4];
  parsed.matrix.ty = m[5];
  parsed.valid = !parsed.matrix.isIdentity();
  return true;
}

// `matrix3d(m11..m44)` carries a column-major 4x4 transform. PAGX layers are 2D, so we project
// the matrix orthographically onto the screen plane by dropping the Z row/column: the visible
// affine is the 2x2 in-plane block plus the in-plane translation
//     a=m11(parts[0]) b=m12(parts[1]) c=m21(parts[4]) d=m22(parts[5]) tx=m41(parts[12]) ty=m42(parts[13]).
// For matrices without perspective (m14=m24=m34=0, m44=1) this projection is exact — it equals
// what the browser composites for a flat element. When the matrix DOES carry perspective the
// orthographic projection is only an approximation (it ignores the perspective divide and any
// Z-ordering between preserve-3d siblings), so we keep the affine subset but emit a warning.
bool ParseMatrix3D(const TransformParseCtx& ctx, HTMLTransform& parsed) {
  if (ctx.parts.size() != 16) {
    ctx.diagnostics.warn("html: matrix3d expects 16 numeric arguments; got '" + ctx.transform +
                         "'");
    return false;
  }
  float m[16];
  for (size_t i = 0; i < 16; i++) {
    if (!ParseScalarFloat(ctx.parts[i], m[i])) {
      ctx.diagnostics.warn("html: matrix3d argument '" + ctx.parts[i] +
                           "' is not a number; ignored");
      return false;
    }
  }
  if (m[3] != 0.0f || m[7] != 0.0f || m[11] != 0.0f || m[15] != 1.0f) {
    ctx.diagnostics.warn("html: matrix3d '" + ctx.transform +
                         "' carries perspective; projecting onto its 2D affine subset (Z-depth "
                         "and perspective divide are not represented)");
  }
  parsed.matrix.a = m[0];
  parsed.matrix.b = m[1];
  parsed.matrix.c = m[4];
  parsed.matrix.d = m[5];
  parsed.matrix.tx = m[12];
  parsed.matrix.ty = m[13];
  parsed.valid = !parsed.matrix.isIdentity();
  return true;
}

// `parseBoxTransform` dispatch table. `composeMatrix` is true when the discrete-field
// composition (`T * R * Skew * S`) should run after the handler — `matrix()` and `matrix3d()`
// are the false cases because they have already populated `parsed.matrix` directly.
struct TransformHandlerEntry {
  const char* name;
  TransformHandler handler;
  bool composeMatrix;
};

const TransformHandlerEntry TRANSFORM_HANDLERS[] = {
    {"skewx", &ParseSkewX, true},           {"skewy", &ParseSkewY, true},
    {"rotate", &ParseRotate, true},         {"scale", &ParseScale, true},
    {"scalex", &ParseScaleX, true},         {"scaley", &ParseScaleY, true},
    {"translate", &ParseTranslate, true},   {"translatex", &ParseTranslateX, true},
    {"translatey", &ParseTranslateY, true}, {"matrix", &ParseMatrix, false},
    {"matrix3d", &ParseMatrix3D, false},
};

const TransformHandlerEntry* FindTransformHandler(const std::string& fn) {
  for (const auto& entry : TRANSFORM_HANDLERS) {
    if (fn == entry.name) return &entry;
  }
  return nullptr;
}

// CSS edge-overlap scaling: when the sum of two border radii exceeds the box edge they
// share, every radius shrinks by the smallest factor that keeps every edge within bounds.
// `scale` carries the running minimum across all four edges.
void ClampBorderRadiusScale(float sum, float edge, float& scale) {
  if (sum > edge && sum > 0.0f) {
    float fe = edge / sum;
    if (fe < scale) scale = fe;
  }
}

// Parses a CSS 1-4 token px shorthand (used by `padding` and `margin`). Each whitespace-separated
// token is resolved through `parseAbsoluteLengthPx`; tokens that fail to parse are reported with
// `propName` and skipped (the corresponding slot stays at the shorthand-expansion default of 0).
// Returns false when no usable token survived; otherwise fills `out` via `ExpandFourSideShorthand`.
bool ParsePxShorthandTokens(const std::string& raw, const char* propName,
                            HTMLValueParser& valueParser, HTMLDiagnosticSink& diagnostics,
                            FourSidedValue& out) {
  auto tokens = SplitTopLevelWhitespace(raw);
  std::vector<float> nums;
  nums.reserve(tokens.size());
  for (auto& t : tokens) {
    float v = valueParser.parseAbsoluteLengthPx(t);
    if (std::isnan(v)) {
      diagnostics.warn(std::string("html: invalid ") + propName + " token '" + t + "'");
      continue;
    }
    nums.push_back(v);
  }
  if (nums.empty()) return false;
  out = ExpandFourSideShorthand(nums);
  return true;
}

}  // namespace

HTMLStyleCascade::HTMLStyleCascade(HTMLDiagnosticSink& sink, HTMLValueParser& valueParser)
    : _diagnostics(sink), _valueParser(valueParser) {
}

void HTMLStyleCascade::setFontFallbackSink(FontFallbackThunk thunk, void* userData) {
  _fontFallbackThunk = thunk;
  _fontFallbackUserData = userData;
}

void HTMLStyleCascade::collectStyles(const std::shared_ptr<DOMNode>& head) {
  auto child = head->getFirstChild();
  while (child) {
    if (child->type == DOMNodeType::Element) {
      if (child->name == "style") {
        parseStyleBlock(child);
      } else if (child->name != "title" && child->name != "meta" && child->name != "link") {
        _diagnostics.warn("html: element '<" + child->name +
                          ">' inside <head> is not allowed; skipped");
      } else if (child->name == "link") {
        auto* rel = child->findAttribute("rel");
        if (rel && ToLower(Trim(*rel)) == "stylesheet") {
          _diagnostics.warn("html: external <link rel=\"stylesheet\"> is not supported; ignored");
        }
        // Other <link> uses (preconnect/icon/etc.) are silently ignored.
      }
    }
    child = child->getNextSibling();
  }
}

void HTMLStyleCascade::parseStyleBlock(const std::shared_ptr<DOMNode>& styleNode) {
  // The text content is stored as a child Text node whose `name` field carries the text.
  auto textChild = styleNode->getFirstChild();
  if (!textChild || textChild->type != DOMNodeType::Text) {
    return;
  }
  std::vector<std::string> droppedAtRules;
  auto rules = TokenizeStyleSheet(textChild->name, droppedAtRules);
  for (auto& at : droppedAtRules) {
    _diagnostics.warn("html: at-rule '" + at + "' not supported in <style>; dropped");
  }
  for (auto& rule : rules) {
    if (rule.declarations.empty()) continue;
    auto selectors = SplitTopLevelCommas(rule.selectors);
    for (auto& rawSel : selectors) {
      std::string sel = Trim(rawSel);
      if (sel.empty()) continue;
      ParsedSelector parsed = ClassifySelector(sel);
      if (parsed.kind == SelectorKind::Universal) {
        _diagnostics.warn(
            "html: universal selector '*' not allowed in <style>; declarations dropped");
        continue;
      }
      // Parse each rule's declarations once at <style> collection time so per-element
      // resolution can copy the resulting PropertyMap instead of re-parsing the same string.
      if (parsed.kind == SelectorKind::Class) {
        ParseStyleString(rule.declarations, _classRules[parsed.key]);
        continue;
      }
      if (parsed.kind == SelectorKind::Element) {
        ParseStyleString(rule.declarations, _elementRules[parsed.key]);
        continue;
      }
      _diagnostics.warn("html: unsupported selector '" + sel +
                        "' in <style>; declarations dropped");
    }
  }
}

void HTMLStyleCascade::mergeClassRules(const std::string& classAttribute, PropertyMap& out) {
  size_t p = 0;
  const auto& s = classAttribute;
  while (p < s.size()) {
    while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) p++;
    if (p >= s.size()) break;
    size_t e = p;
    while (e < s.size() && !std::isspace(static_cast<unsigned char>(s[e]))) e++;
    auto it = _classRules.find(s.substr(p, e - p));
    if (it != _classRules.end()) {
      // Last writer wins, matching the inline-style cascade order.
      for (const auto& kv : it->second) {
        out[kv.first] = kv.second;
      }
    }
    p = e;
  }
}

const HTMLStyleCascade::PropertyMap& HTMLStyleCascade::getResolvedStyle(
    const std::shared_ptr<DOMNode>& node) {
  auto it = _resolvedCache.find(node.get());
  if (it != _resolvedCache.end()) {
    return it->second;
  }
  auto& slot = _resolvedCache[node.get()];

  // Priority: element defaults -> element rules from <style> -> class rules -> inline.
  const auto& tagDefaults = ParsedElementDefaults();
  auto tagIt = tagDefaults.find(node->name);
  if (tagIt != tagDefaults.end()) {
    for (const auto& kv : tagIt->second) {
      slot[kv.first] = kv.second;
    }
  }
  auto elemRuleIt = _elementRules.find(node->name);
  if (elemRuleIt != _elementRules.end()) {
    for (const auto& kv : elemRuleIt->second) {
      slot[kv.first] = kv.second;
    }
  }
  auto* classAttr = node->findAttribute("class");
  if (classAttr) {
    mergeClassRules(*classAttr, slot);
  }
  auto* styleAttr = node->findAttribute("style");
  if (styleAttr) {
    ParseStyleString(*styleAttr, slot);
  }
  return slot;
}

std::string HTMLStyleCascade::getStyleProperty(const std::shared_ptr<DOMNode>& node,
                                               const std::string& property,
                                               const std::string& fallback) {
  const auto& props = getResolvedStyle(node);
  auto it = props.find(property);
  if (it != props.end()) return it->second;
  return fallback;
}

HTMLInheritedStyle HTMLStyleCascade::resolveInheritedStyle(const std::shared_ptr<DOMNode>& element,
                                                           const HTMLInheritedStyle& parent) {
  HTMLInheritedStyle out = parent;
  const auto& props = getResolvedStyle(element);
  CopyProperty(props, "color", out.color);
  CopyProperty(props, "font-family", out.fontFamily);
  // Re-parse the CSS font-family stack only when the raw value actually changed at this
  // element. The cascade (`out = parent`) already carries the parent's parsed stack
  // through unchanged in the common case where descendants inherit verbatim.
  if (out.fontFamily != parent.fontFamily ||
      (parent.fontFamilyChain.empty() && !out.fontFamily.empty())) {
    out.fontFamilyChain.clear();
    out.primaryFontFamily.clear();
    auto tokens = html::ParseFontFamilyTokens(out.fontFamily);
    for (auto& token : tokens) {
      std::string resolved = html::ResolveGenericFontFamily(token);
      if (resolved.empty()) {
        // Recognised generic with no concrete mapping on this platform (cursive,
        // fantasy, ...). Drop with a single diagnostic so a stack like
        // `cursive, "Foo"` still falls through to "Foo".
        _diagnostics.warn("html: font-family generic '" + token +
                          "' not mapped on this platform; dropped");
        continue;
      }
      out.fontFamilyChain.push_back(resolved);
    }
    if (!out.fontFamilyChain.empty()) {
      out.primaryFontFamily = out.fontFamilyChain.front();
    }
    if (_fontFallbackThunk) {
      _fontFallbackThunk(_fontFallbackUserData, out.fontFamilyChain);
    }
  }
  CopyProperty(props, "font-size", out.fontSize);
  CopyProperty(props, "font-weight", out.fontWeight);
  CopyProperty(props, "font-style", out.fontStyle);
  CopyProperty(props, "letter-spacing", out.letterSpacing);
  CopyProperty(props, "line-height", out.lineHeight);
  CopyProperty(props, "text-align", out.textAlign);
  CopyProperty(props, "text-decoration", out.textDecoration);
  CopyProperty(props, "text-decoration-color", out.textDecorationColor);
  CopyProperty(props, "white-space", out.whiteSpace);
  CopyProperty(props, "writing-mode", out.writingMode);
  // Propagate gradient text fill from the nearest clip-to-text ancestor. `out.textFillImage`
  // already inherits the parent's value via `out = parent`; we only override when this element
  // itself sets `background-clip: text` together with a gradient `background-image`.
  std::string ownBgImage = LookupProperty(props, "background-image");
  if (ownBgImage.empty()) {
    const std::string& sh = LookupProperty(props, "background");
    if (!sh.empty() && sh.find("gradient") != std::string::npos) {
      ownBgImage = sh;
    }
  }
  if (LookupLowerTrimmed(props, "background-clip") == "text" && !ownBgImage.empty() &&
      ownBgImage.find("gradient") != std::string::npos) {
    out.textFillImage = ownBgImage;
  }
  // Split the CSS font-weight / font-style request into the real-face style label PAGX Text
  // resolves plus the synthetic (faux) axes the renderer embosses on top. Bold (weight >= 600) and
  // italic/oblique are baked as faux flags and dropped from the label so an uninstalled web face
  // (e.g. "Noto Sans SC Black Italic") still renders at the authored weight and slant instead of
  // collapsing to a thin upright fallback. Lighter weights (Light / Medium) cannot be synthesised
  // and stay in the label.
  FontStyleSynthesis fontSynthesis = ResolveFontStyleSynthesis(out.fontWeight, out.fontStyle);
  out.fontStyleName = fontSynthesis.fontStyleName;
  out.fauxBold = fontSynthesis.fauxBold;
  out.fauxItalic = fontSynthesis.fauxItalic;

  static const char* TextDisallowed[] = {
      "text-transform", "text-indent",  "word-spacing", "unicode-bidi",
      "font-variant",   "font-stretch", "font"};
  for (const auto* prop : TextDisallowed) {
    if (!LookupProperty(props, prop).empty()) {
      _diagnostics.warn(std::string("html: ") + prop + " not supported; ignored");
    }
  }
  std::string textOverflow = LookupLowerTrimmed(props, "text-overflow");
  if (textOverflow == "ellipsis") {
    _diagnostics.warn("html: text-overflow: ellipsis is not implemented in PAGX");
  }

  // Refresh the pre-resolved numeric forms so text-leaf conversion can read them directly
  // without re-parsing strings for every fragment.
  if (out.fontSize.empty()) {
    out.fontSizePx = HTML_DEFAULT_FONT_SIZE;
  } else {
    float fontSizePx = _valueParser.parseAbsoluteLengthPx(out.fontSize);
    if (std::isnan(fontSizePx) || fontSizePx <= 0) fontSizePx = HTML_DEFAULT_FONT_SIZE;
    out.fontSizePx = fontSizePx;
  }
  if (out.letterSpacing.empty()) {
    out.letterSpacingPx = 0.0f;
  } else {
    float ls = _valueParser.parseAbsoluteLengthPx(out.letterSpacing);
    out.letterSpacingPx = std::isnan(ls) ? 0.0f : ls;
  }
  out.resolvedTextColor =
      _valueParser.parseColor(out.color.empty() ? std::string(HTML_DEFAULT_TEXT_COLOR) : out.color);
  return out;
}

HTMLBoxAttributes HTMLStyleCascade::computeBoxAttributes(const std::shared_ptr<DOMNode>& element) {
  HTMLBoxAttributes box = {};
  const auto& props = getResolvedStyle(element);
  parseBoxSizing(box, props);
  parseBoxPositioning(box, props);
  parseBoxLayout(box, props);
  parseBoxVisuals(box, props);
  parseBoxTransform(box, props);
  return box;
}

void HTMLStyleCascade::parseBoxSizing(HTMLBoxAttributes& box, const PropertyMap& props) {
  ParseSizingDimension(LookupProperty(props, "width"), box.widthPx, box.widthPct);
  ParseSizingDimension(LookupProperty(props, "height"), box.heightPx, box.heightPct);
  std::string boxSizing = LookupLowerTrimmed(props, "box-sizing");
  if (!boxSizing.empty() && boxSizing != "border-box") {
    _diagnostics.warn("html: box-sizing: " + boxSizing + " not supported; treated as border-box");
  }
  static const char* SizingDisallowed[] = {"min-width", "min-height", "max-width", "max-height",
                                           "aspect-ratio"};
  for (const auto* prop : SizingDisallowed) {
    if (!LookupProperty(props, prop).empty()) {
      _diagnostics.warn(std::string("html: ") + prop + " not supported; ignored");
    }
  }
}

void HTMLStyleCascade::parseBoxPositioning(HTMLBoxAttributes& box, const PropertyMap& props) {
  std::string pos = LookupLowerTrimmed(props, "position");
  if (pos == "absolute") {
    box.absolute = true;
  } else if (!pos.empty() && pos != "static") {
    _diagnostics.warn("html: position: " + pos + " not supported; downgraded to absolute");
    box.absolute = true;
  }
  if (!box.absolute) return;
  const std::string& left = LookupProperty(props, "left");
  if (!left.empty()) box.leftPx = _valueParser.parseAbsoluteLengthPx(left);
  const std::string& right = LookupProperty(props, "right");
  if (!right.empty()) box.rightPx = _valueParser.parseAbsoluteLengthPx(right);
  const std::string& top = LookupProperty(props, "top");
  if (!top.empty()) box.topPx = _valueParser.parseAbsoluteLengthPx(top);
  const std::string& bottom = LookupProperty(props, "bottom");
  if (!bottom.empty()) box.bottomPx = _valueParser.parseAbsoluteLengthPx(bottom);
}

void HTMLStyleCascade::applyMarginLonghand(const PropertyMap& props, const char* propName,
                                           float& outPx) {
  const std::string& v = LookupProperty(props, propName);
  if (v.empty()) return;
  float n = _valueParser.parseAbsoluteLengthPx(v);
  if (std::isnan(n)) {
    _diagnostics.warn(std::string("html: invalid ") + propName + " value '" + v + "'");
    return;
  }
  outPx = n;
}

void HTMLStyleCascade::parseBoxLayout(HTMLBoxAttributes& box, const PropertyMap& props) {
  std::string disp = LookupLowerTrimmed(props, "display");
  if (disp == "flex") {
    box.displayFlex = true;
  } else if (!disp.empty() && disp != "block" && disp != "inline" && disp != "inline-block") {
    _diagnostics.warn("html: display: " + disp + " not supported; ignored");
  } else if (disp == "inline-block") {
    _diagnostics.warn("html: display: inline-block not supported; treated as block");
  }
  std::string fd = LookupLowerTrimmed(props, "flex-direction");
  if (fd == "column" || fd == "column-reverse") {
    box.flexRow = false;
    if (fd == "column-reverse") {
      _diagnostics.warn("html: flex-direction: column-reverse not supported");
    }
  } else if (fd == "row-reverse") {
    _diagnostics.warn("html: flex-direction: row-reverse not supported");
  }
  const std::string& gap = LookupProperty(props, "gap");
  if (!gap.empty()) {
    box.gapPx = _valueParser.parseAbsoluteLengthPx(gap);
    box.gapSet = !std::isnan(box.gapPx);
    if (!box.gapSet) box.gapPx = 0;
  }
  const std::string& padding = LookupProperty(props, "padding");
  if (!padding.empty()) {
    FourSidedValue p = {};
    if (ParsePxShorthandTokens(padding, "padding", _valueParser, _diagnostics, p)) {
      box.padding.top = p.top;
      box.padding.right = p.right;
      box.padding.bottom = p.bottom;
      box.padding.left = p.left;
      box.paddingSet = true;
    }
  }
  std::string ai = LookupLowerTrimmed(props, "align-items");
  if (!ai.empty()) box.alignItems = ai;
  std::string jc = LookupLowerTrimmed(props, "justify-content");
  if (!jc.empty()) box.justifyContent = jc;
  const std::string& flex = LookupProperty(props, "flex");
  if (!flex.empty()) {
    std::string trimmed = Trim(flex);
    char* end = nullptr;
    float v = std::strtof(trimmed.c_str(), &end);
    bool consumedAll = end != trimmed.c_str() && Trim(end).empty();
    if (consumedAll && v >= 0) {
      box.flexGrow = v;
      box.flexGrowSet = true;
    } else {
      _diagnostics.warn("html: flex shorthand '" + flex + "' not supported beyond 'flex: N'");
    }
  }
  if (!LookupProperty(props, "flex-wrap").empty()) {
    _diagnostics.warn("html: flex-wrap not supported; ignored");
  }
  static const char* LayoutDisallowed[] = {"flex-grow", "flex-shrink",   "flex-basis", "float",
                                           "order",     "align-content", "align-self", "direction"};
  for (const auto* prop : LayoutDisallowed) {
    if (!LookupProperty(props, prop).empty()) {
      _diagnostics.warn(std::string("html: ") + prop + " not supported; ignored");
    }
  }
  if (!LookupProperty(props, "margin").empty() || !LookupProperty(props, "margin-top").empty() ||
      !LookupProperty(props, "margin-right").empty() ||
      !LookupProperty(props, "margin-bottom").empty() ||
      !LookupProperty(props, "margin-left").empty()) {
    // CSS `margin` shorthand: 1-4 length tokens (T / TB,RL / T,RL,B / T,R,B,L). Longhands
    // override individual sides after the shorthand expansion. The subset transformer has
    // already normalised every accepted unit to plain px, so any token that fails to parse
    // here is malformed input and is reported and skipped (the corresponding side stays at
    // its default 0). Resulting per-side values are routed onto positioning / padding by
    // `wrapForMargin` at apply time — PAGX has no margin field on Layer / LayoutNode.
    const std::string& margin = LookupProperty(props, "margin");
    if (!margin.empty()) {
      FourSidedValue m = {};
      if (ParsePxShorthandTokens(margin, "margin", _valueParser, _diagnostics, m)) {
        box.marginTopPx = m.top;
        box.marginRightPx = m.right;
        box.marginBottomPx = m.bottom;
        box.marginLeftPx = m.left;
      }
    }
    applyMarginLonghand(props, "margin-top", box.marginTopPx);
    applyMarginLonghand(props, "margin-right", box.marginRightPx);
    applyMarginLonghand(props, "margin-bottom", box.marginBottomPx);
    applyMarginLonghand(props, "margin-left", box.marginLeftPx);
  }
  if (!LookupProperty(props, "grid-template-columns").empty()) {
    _diagnostics.warn("html: grid layout not supported");
  }
  if (!LookupProperty(props, "grid-template-rows").empty()) {
    _diagnostics.warn("html: grid layout not supported");
  }
}

void HTMLStyleCascade::parseBoxVisuals(HTMLBoxAttributes& box, const PropertyMap& props) {
  std::string bgColor = LookupProperty(props, "background-color");
  if (bgColor.empty()) {
    bgColor = LookupProperty(props, "background");  // accept shorthand if it's color-only
  }
  // `parseColor` accepts hex, named colors, and `rgb()/rgba()` literals. We only need to bail
  // out when the value is actually a non-color shorthand (gradient / url-image).
  if (!bgColor.empty() && bgColor.find("gradient") == std::string::npos &&
      bgColor.find("url(") == std::string::npos) {
    box.backgroundColor = _valueParser.parseColor(bgColor);
    box.backgroundColorSet = true;
  }
  std::string bgImage = LookupProperty(props, "background-image");
  if (bgImage.empty()) {
    const std::string& sh = LookupProperty(props, "background");
    if (!sh.empty() && sh.find("gradient") != std::string::npos) {
      bgImage = sh;
    }
  }
  if (!bgImage.empty()) {
    box.backgroundImage = bgImage;
  }
  // A `url(...)` background round-trips through `ImagePattern`, which needs the CSS fitting
  // hints the exporter emitted alongside it. Capture them only for url backgrounds; gradients
  // ignore size/repeat/position entirely.
  if (!bgImage.empty() && ToLower(bgImage).find("url(") != std::string::npos) {
    box.backgroundSize = LookupLowerTrimmed(props, "background-size");
    box.backgroundRepeat = LookupLowerTrimmed(props, "background-repeat");
    box.backgroundPosition = LookupLowerTrimmed(props, "background-position");
  }
  // `background-clip: text` is the only clip value the importer models. The subset transformer
  // already normalises every other keyword to empty, so a non-empty value here equals `text`.
  std::string bgClip = LookupLowerTrimmed(props, "background-clip");
  if (bgClip == "text") {
    box.backgroundClipText = true;
  }

  const std::string& br = LookupProperty(props, "border-radius");
  if (!br.empty()) {
    parseBorderRadius(box, props);
  }

  const std::string& border = LookupProperty(props, "border");
  if (!border.empty()) {
    parseBorder(box, border);
  }

  box.boxShadow = LookupProperty(props, "box-shadow");
  box.filter = LookupProperty(props, "filter");
  box.backdropFilter = LookupProperty(props, "backdrop-filter");

  // Alpha / luminance mask descriptors. `mask-image` is kept raw (the `url(data:...)` wrapper is
  // stripped at apply time); the rest are lower-cased so keyword comparisons are case-insensitive.
  box.maskImage = LookupProperty(props, "mask-image");
  box.maskMode = LookupLowerTrimmed(props, "mask-mode");
  box.maskSize = LookupLowerTrimmed(props, "mask-size");
  box.maskPosition = LookupLowerTrimmed(props, "mask-position");
  // `clip-path: url(#id)` reference, kept raw so the apply-side can extract the id.
  box.clipPathRef = LookupProperty(props, "clip-path");

  const std::string& op = LookupProperty(props, "opacity");
  if (!op.empty()) {
    char* end = nullptr;
    float v = std::strtof(op.c_str(), &end);
    if (end != op.c_str()) {
      box.opacity = std::max(0.0f, std::min(1.0f, v));
      box.opacitySet = true;
    }
  }
  box.mixBlendMode = LookupLowerTrimmed(props, "mix-blend-mode");

  std::string overflow = LookupLowerTrimmed(props, "overflow");
  if (overflow == "hidden") {
    box.clipOverflow = true;
  } else if (!overflow.empty() && overflow != "visible") {
    _diagnostics.warn("html: overflow: " + overflow + " not fully supported");
  }

  box.objectFit = LookupLowerTrimmed(props, "object-fit");

  static const char* VisualsDisallowed[] = {"outline", "perspective"};
  for (const auto* prop : VisualsDisallowed) {
    if (!LookupProperty(props, prop).empty()) {
      _diagnostics.warn(std::string("html: ") + prop + " not supported; ignored");
    }
  }
}

void HTMLStyleCascade::parseBoxTransform(HTMLBoxAttributes& box, const PropertyMap& props) {
  // CSS `transform-origin` resolved by Chromium's computed style is almost
  // always emitted as `<x>px <y>px` (the centre of the element box, the CSS
  // default). Hand-written subset HTML still sometimes carries `50% 50%` or
  // `center [center]`; both are accepted as synonyms for the box centre.
  // Other percentage / keyword combinations cannot be resolved without
  // knowing the box width/height in absolute terms (parseBoxSizing fills
  // those in `box.widthPx/heightPx`), so non-px / non-default forms are
  // warned and the transform falls back to the centre pivot.
  std::string origin = ToLower(Trim(LookupProperty(props, "transform-origin")));
  float resolvedOriginX = NAN;
  float resolvedOriginY = NAN;
  bool originIsCentre =
      origin.empty() || origin == "50% 50%" || origin == "center" || origin == "center center";
  if (!originIsCentre) {
    std::vector<std::string> originParts;
    size_t spaceIdx = origin.find(' ');
    if (spaceIdx != std::string::npos) {
      originParts.push_back(Trim(origin.substr(0, spaceIdx)));
      originParts.push_back(Trim(origin.substr(spaceIdx + 1)));
    }
    if (originParts.size() == 2) {
      float ox = _valueParser.parseAbsoluteLengthPx(originParts[0]);
      float oy = _valueParser.parseAbsoluteLengthPx(originParts[1]);
      if (!std::isnan(ox) && !std::isnan(oy)) {
        // Suppress the warning when the px values are exactly the box's
        // geometric centre — Chromium emits this for the CSS default and
        // applyBoxTransform's fallback pivot already lands there.
        bool matchesCentre = !std::isnan(box.widthPx) && !std::isnan(box.heightPx) &&
                             std::fabs(ox - box.widthPx * 0.5f) < 0.5f &&
                             std::fabs(oy - box.heightPx * 0.5f) < 0.5f;
        if (!matchesCentre) {
          resolvedOriginX = ox;
          resolvedOriginY = oy;
        }
        originIsCentre = true;  // resolved to a usable pivot, no warning needed
      }
    }
    if (!originIsCentre) {
      _diagnostics.warn("html: transform-origin '" + origin +
                        "' is not supported; assuming default '50% 50%'");
    }
  }

  std::string transform = Trim(LookupProperty(props, "transform"));
  if (transform.empty()) return;

  std::string fn;
  std::string args;
  if (!ParseCssFunctionCall(transform, fn, args)) {
    _diagnostics.warn("html: transform '" + transform + "' is malformed; ignored");
    return;
  }
  auto parts = SplitCommaArgs(args);
  HTMLTransform parsed = {};

  const TransformHandlerEntry* entry = FindTransformHandler(fn);
  if (entry == nullptr) {
    _diagnostics.warn("html: transform function '" + fn + "' is not in the supported subset");
    return;
  }
  TransformParseCtx ctx{transform, parts, _diagnostics, _valueParser};
  if (!entry->handler(ctx, parsed)) return;

  // Compose the discrete-field representation into `parsed.matrix` for the
  // single-function branches. Skipped for the `matrix(...)` branch (which already populated
  // `parsed.matrix` directly and intentionally left the discrete fields at their defaults),
  // and for the case where the handler bailed early. Order mirrors the CSS spec:
  // T(translate) * R(rotation) * Skew(skew, skewAxis) * S(scale). `transform-origin` is NOT
  // folded in here; the Layer-side consumer applies it as `T(cx, cy) * matrix * T(-cx, -cy)`.
  if (parsed.valid && entry->composeMatrix) {
    Matrix translate = Matrix::Translate(parsed.translateX, parsed.translateY);
    Matrix rotate =
        (parsed.rotation != 0.0f) ? Matrix::Rotate(parsed.rotation) : Matrix::Identity();
    Matrix skew = Matrix::Identity();
    if (parsed.skew != 0.0f) {
      // PAGX skew is parameterised as (skew, skewAxis) where skewAxis is the angle of the
      // axis that stays fixed. The CSS-equivalent affine is the product of an axis
      // rotation, an x-shear by tan(skew), and the inverse axis rotation:
      //     R(skewAxis) * Shear(tan(skew)) * R(-skewAxis).
      // For the two CSS forms we accept (skewX/skewY, populated by parseBoxTransform
      // with skewAxis=0/90), this reduces to a pure horizontal/vertical shear.
      float radians = parsed.skew * 3.14159265358979323846f / 180.0f;
      float t = std::tan(radians);
      Matrix shear = Matrix::Identity();
      if (parsed.skewAxis == 0.0f) {
        shear.c = t;  // horizontal shear (CSS skewY's effect after the sign flip)
      } else if (parsed.skewAxis == 90.0f) {
        shear.b = t;  // vertical shear (CSS skewX's effect after the sign flip)
      } else {
        Matrix axisRotate = Matrix::Rotate(parsed.skewAxis);
        Matrix axisRotateInv = Matrix::Rotate(-parsed.skewAxis);
        shear.c = t;
        skew = axisRotate * shear * axisRotateInv;
      }
      if (skew.isIdentity()) {
        skew = shear;
      }
    }
    Matrix scale = Matrix::Scale(parsed.scaleX, parsed.scaleY);
    parsed.matrix = translate * rotate * skew * scale;
  }

  // Forward the resolved transform-origin (if it differs from the box's
  // centre) onto the parsed transform so applyBoxTransform pivots around
  // the requested point. Centre pivots are signalled by leaving the fields
  // at NaN — the apply path already defaults to W/2,H/2 in that case.
  if (parsed.valid) {
    parsed.originXPx = resolvedOriginX;
    parsed.originYPx = resolvedOriginY;
  }

  box.transform = parsed;
}

// Resolves one axis of a `border-radius` value (the horizontal radii before the optional '/',
// or the vertical radii after it) into four per-corner pixel values in TL/TR/BR/BL order.
// `axisLengthPx` is the box dimension that percentage tokens resolve against (width for the
// horizontal axis, height for the vertical axis). Returns false when any token is invalid or a
// percentage appears without a fixed px size on that axis, recording a diagnostic; on success the
// four corners are written to `out`.
static bool ResolveBorderRadiusAxis(const std::vector<std::string>& tokens, float axisLengthPx,
                                    HTMLValueParser& valueParser, HTMLDiagnosticSink& diagnostics,
                                    FourSidedValue& out) {
  std::vector<float> nums;
  nums.reserve(tokens.size());
  for (auto& t : tokens) {
    std::string trimmed = Trim(t);
    if (trimmed.empty()) {
      continue;
    }
    float v = NAN;
    if (trimmed.back() == '%') {
      char* end = nullptr;
      float pct = std::strtof(trimmed.c_str(), &end);
      if (end == trimmed.c_str()) {
        diagnostics.warn("html: invalid border-radius token '" + t + "'");
        return false;
      }
      if (std::isnan(axisLengthPx)) {
        diagnostics.warn("html: percentage border-radius '" + t +
                         "' requires fixed px width/height on the same element; ignored");
        return false;
      }
      v = pct * axisLengthPx / 100.0f;
    } else {
      v = valueParser.parseAbsoluteLengthPx(trimmed);
      if (std::isnan(v)) {
        diagnostics.warn("html: invalid border-radius token '" + t + "'");
        return false;
      }
    }
    nums.push_back(std::max(v, 0.0f));
  }
  if (nums.empty()) {
    return false;
  }
  out = ExpandFourSideShorthand(nums);
  return true;
}

void HTMLStyleCascade::parseBorderRadius(HTMLBoxAttributes& box, const PropertyMap& props) {
  // CSS `border-radius` shorthand accepts up to 4 lengths or percentages (top-left,
  // top-right, bottom-right, bottom-left), with an optional '/' separating horizontal
  // and vertical radii for elliptical corners. PAGX has two shapes that can carry the
  // result — `Ellipse` (a center+size oval) and `Rectangle` (single uniform `roundness`):
  //   - ellipse case: when every corner's horizontal radius equals half the box width and
  //     every corner's vertical radius equals half the box height, the rounded rectangle
  //     degenerates into an inscribed ellipse. This covers the canonical `border-radius: 50%`
  //     (on both square and non-square boxes) and the explicit `50% / 50%` form, and is the
  //     only configuration in which PAGX can represent a true two-axis radius. Marked with
  //     `borderRadiusEllipse` so the layer builder emits an `Ellipse`.
  //   - elliptical form (containing '/') that is NOT a full ellipse: PAGX has no per-axis
  //     corner-radius primitive, so warn and drop the value.
  //   - 1-4 length / percentage tokens: expand to per-corner (TL, TR, BR, BL). When all four
  //     resolved values are equal the layer builder emits a `Rectangle` with that uniform
  //     `roundness`; when they differ it synthesises a `Path` tracing the rounded outline so
  //     the "single rounded corner" patterns used as corner decorations (e.g. Tailwind
  //     `rounded-bl-full`) round-trip exactly.
  //   - percentage tokens in the non-ellipse single-axis path resolve per corner against
  //     `min(widthPx, heightPx)`. Percentages on elements without fixed px sizes are dropped
  //     with a warning, and the affected corner stays at 0.
  const std::string& br = LookupProperty(props, "border-radius");

  // Split the optional '/' into horizontal and vertical token groups; without a '/' the vertical
  // radii mirror the horizontal ones. Detect the inscribed-ellipse case first, before collapsing
  // to the single-axis representation that loses the per-axis radii.
  auto slashPos = br.find('/');
  std::string hPart = slashPos == std::string::npos ? br : br.substr(0, slashPos);
  std::string vPart = slashPos == std::string::npos ? br : br.substr(slashPos + 1);
  auto hTokens = SplitTopLevelWhitespace(hPart);
  auto vTokens = SplitTopLevelWhitespace(vPart);
  if (!std::isnan(box.widthPx) && !std::isnan(box.heightPx) && box.widthPx > 0 &&
      box.heightPx > 0) {
    FourSidedValue h{};
    FourSidedValue v{};
    if (ResolveBorderRadiusAxis(hTokens, box.widthPx, _valueParser, _diagnostics, h) &&
        ResolveBorderRadiusAxis(vTokens, box.heightPx, _valueParser, _diagnostics, v)) {
      float halfW = box.widthPx * 0.5f;
      float halfH = box.heightPx * 0.5f;
      bool hIsHalf = pag::FloatNearlyEqual(h.top, halfW) && pag::FloatNearlyEqual(h.right, halfW) &&
                     pag::FloatNearlyEqual(h.bottom, halfW) && pag::FloatNearlyEqual(h.left, halfW);
      bool vIsHalf = pag::FloatNearlyEqual(v.top, halfH) && pag::FloatNearlyEqual(v.right, halfH) &&
                     pag::FloatNearlyEqual(v.bottom, halfH) && pag::FloatNearlyEqual(v.left, halfH);
      if (hIsHalf && vIsHalf) {
        box.borderRadiusTLPx = halfW;
        box.borderRadiusTRPx = halfW;
        box.borderRadiusBRPx = halfW;
        box.borderRadiusBLPx = halfW;
        box.borderRadiusUniform = true;
        box.borderRadiusEllipse = true;
        box.borderRadiusSet = true;
        return;
      }
    }
  }

  if (slashPos != std::string::npos) {
    _diagnostics.warn("html: elliptical border-radius '" + br +
                      "' not supported by PAGX Rectangle; ignored");
    return;
  }
  auto tokens = hTokens;
  std::vector<float> nums;
  nums.reserve(tokens.size());
  for (auto& t : tokens) {
    std::string trimmed = Trim(t);
    float v = NAN;
    if (!trimmed.empty() && trimmed.back() == '%') {
      char* end = nullptr;
      float pct = std::strtof(trimmed.c_str(), &end);
      if (end == trimmed.c_str()) {
        _diagnostics.warn("html: invalid border-radius token '" + t + "'");
        continue;
      }
      if (std::isnan(box.widthPx) || std::isnan(box.heightPx)) {
        _diagnostics.warn("html: percentage border-radius '" + t +
                          "' requires fixed px width/height on the same element; ignored");
        continue;
      }
      v = pct * std::min(box.widthPx, box.heightPx) / 100.0f;
    } else {
      v = _valueParser.parseAbsoluteLengthPx(trimmed);
      if (std::isnan(v)) {
        _diagnostics.warn("html: invalid border-radius token '" + t + "'");
        continue;
      }
    }
    nums.push_back(v);
  }
  if (nums.empty()) {
    return;
  }

  // CSS shorthand expansion. We tolerate the irregular 0/empty-token case by padding with 0s
  // so a malformed input doesn't lose unrelated corner data. Slot mapping for `border-radius`
  // is top-left / top-right / bottom-right / bottom-left.
  FourSidedValue rs = ExpandFourSideShorthand(nums);
  float tl = std::max(rs.top, 0.0f);
  float tr = std::max(rs.right, 0.0f);
  float brad = std::max(rs.bottom, 0.0f);
  float bl = std::max(rs.left, 0.0f);

  // CSS edge-overlap clamp: shrink all radii uniformly so the sum of the two radii on any
  // edge never exceeds that edge's length. Pre-clamp values like `9999px` collapse to the
  // box dimension here (so `rounded-bl-full` on a 380x380 box yields BL=380, not 9999).
  if (!std::isnan(box.widthPx) && !std::isnan(box.heightPx) && box.widthPx > 0 &&
      box.heightPx > 0) {
    float scale = 1.0f;
    ClampBorderRadiusScale(tl + tr, box.widthPx, scale);
    ClampBorderRadiusScale(bl + brad, box.widthPx, scale);
    ClampBorderRadiusScale(tl + bl, box.heightPx, scale);
    ClampBorderRadiusScale(tr + brad, box.heightPx, scale);
    if (scale < 1.0f) {
      tl *= scale;
      tr *= scale;
      brad *= scale;
      bl *= scale;
    }
  }
  box.borderRadiusTLPx = tl;
  box.borderRadiusTRPx = tr;
  box.borderRadiusBRPx = brad;
  box.borderRadiusBLPx = bl;
  box.borderRadiusUniform = (tl == tr) && (tr == brad) && (brad == bl);
  // Mirror the legacy behaviour: any successfully parsed border-radius (even all zeros)
  // marks the box as "carrying border-radius styling". Downstream `hasBackgroundVisuals`
  // uses this to decide whether the layer needs a background geometry; emitting a
  // `Rectangle roundness=0` for an all-zero shorthand is harmless and keeps the legacy
  // tests stable.
  box.borderRadiusSet = true;
}

void HTMLStyleCascade::parseBorder(HTMLBoxAttributes& box, const std::string& border) {
  auto tokens = SplitTopLevelWhitespace(border);
  // Collect candidate colour tokens — anything that's not a width / known style keyword.
  // Take the FIRST candidate as the border colour and warn on any subsequent ones so the
  // author notices `border: 1px solid red blue`-style mistakes (previously the second
  // colour silently overwrote the first).
  std::string colorToken;
  bool sawExtraColor = false;
  for (auto& t : tokens) {
    float w = _valueParser.parseAbsoluteLengthPx(t);
    if (!std::isnan(w)) {
      box.borderWidthPx = w;
      continue;
    }
    std::string lt = ToLower(t);
    if (lt == "solid" || lt == "none") continue;
    if (lt == "dashed") {
      box.borderStyle = BorderStyle::Dashed;
      continue;
    }
    if (lt == "dotted") {
      box.borderStyle = BorderStyle::Dotted;
      continue;
    }
    if (lt == "double" || lt == "groove" || lt == "ridge" || lt == "inset" || lt == "outset") {
      _diagnostics.warn("html: border style '" + lt + "' not supported; treated as solid");
      continue;
    }
    if (colorToken.empty()) {
      colorToken = t;
    } else {
      sawExtraColor = true;
    }
  }
  if (sawExtraColor) {
    _diagnostics.warn("html: border value '" + border +
                      "' has multiple colour tokens; first one used");
  }
  if (!colorToken.empty()) {
    box.borderColor = _valueParser.parseColor(colorToken);
  }
  box.borderSet = box.borderWidthPx > 0;
}

}  // namespace pagx
