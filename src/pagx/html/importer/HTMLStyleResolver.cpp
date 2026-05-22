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
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include "pagx/html/importer/HTMLCssCascade.h"
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLParserContext.h"
#include "pagx/utils/CSSFontStyle.h"

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
// (e.g. `HTMLImporter::Options::autoNormalize == false`), so we re-check the shape here:
// the FIRST `)` must coincide with the LAST `)` and trail only whitespace, otherwise the
// value is a compound chain and is rejected.
bool SplitTransformFunction(const std::string& trimmed, std::string& fnName,
                            std::string& argsBody) {
  size_t openParen = trimmed.find('(');
  if (openParen == std::string::npos || openParen == 0) return false;
  size_t closeParen = trimmed.find_last_of(')');
  if (closeParen == std::string::npos || closeParen <= openParen) return false;
  if (trimmed.find(')') != closeParen) return false;
  if (!Trim(trimmed.substr(closeParen + 1)).empty()) return false;
  fnName = ToLower(Trim(trimmed.substr(0, openParen)));
  argsBody = Trim(trimmed.substr(openParen + 1, closeParen - openParen - 1));
  return !fnName.empty();
}

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

}  // namespace

void HTMLParserContext::collectStyles(const std::shared_ptr<DOMNode>& head) {
  auto child = head->getFirstChild();
  while (child) {
    if (child->type == DOMNodeType::Element) {
      if (child->name == "style") {
        parseStyleBlock(child);
      } else if (child->name != "title" && child->name != "meta" && child->name != "link") {
        warn("html: element '<" + child->name + ">' inside <head> is not allowed; skipped");
      } else if (child->name == "link") {
        auto* rel = child->findAttribute("rel");
        if (rel && ToLower(Trim(*rel)) == "stylesheet") {
          warn("html: external <link rel=\"stylesheet\"> is not supported; ignored");
        }
        // Other <link> uses (preconnect/icon/etc.) are silently ignored.
      }
    }
    child = child->getNextSibling();
  }
}

void HTMLParserContext::parseStyleBlock(const std::shared_ptr<DOMNode>& styleNode) {
  // The text content is stored as a child Text node whose `name` field carries the text.
  auto textChild = styleNode->getFirstChild();
  if (!textChild || textChild->type != DOMNodeType::Text) {
    return;
  }
  std::vector<std::string> droppedAtRules;
  auto rules = TokenizeStyleSheet(textChild->name, droppedAtRules);
  for (auto& at : droppedAtRules) {
    warn("html: at-rule '" + at + "' not supported in <style>; dropped");
  }
  for (auto& rule : rules) {
    if (rule.declarations.empty()) continue;
    auto selectors = SplitTopLevelCommas(rule.selectors);
    for (auto& rawSel : selectors) {
      std::string sel = Trim(rawSel);
      if (sel.empty()) continue;
      ParsedSelector parsed = ClassifySelector(sel);
      if (parsed.kind == SelectorKind::Universal) {
        warn("html: universal selector '*' not allowed in <style>; declarations dropped");
        continue;
      }
      // Parse each rule's declarations once at <style> collection time so per-element
      // resolution can copy the resulting PropertyMap instead of re-parsing the same string.
      if (parsed.kind == SelectorKind::Class) {
        ParseStyleString(rule.declarations, _cssClassRules[parsed.key]);
        continue;
      }
      if (parsed.kind == SelectorKind::Element) {
        ParseStyleString(rule.declarations, _cssElementRules[parsed.key]);
        continue;
      }
      warn("html: unsupported selector '" + sel + "' in <style>; declarations dropped");
    }
  }
}

void HTMLParserContext::mergeClassRules(const std::string& classAttribute,
                                        std::unordered_map<std::string, std::string>& out) {
  size_t p = 0;
  const auto& s = classAttribute;
  while (p < s.size()) {
    while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) p++;
    if (p >= s.size()) break;
    size_t e = p;
    while (e < s.size() && !std::isspace(static_cast<unsigned char>(s[e]))) e++;
    auto it = _cssClassRules.find(s.substr(p, e - p));
    if (it != _cssClassRules.end()) {
      // Last writer wins, matching the inline-style cascade order.
      for (const auto& kv : it->second) {
        out[kv.first] = kv.second;
      }
    }
    p = e;
  }
}

const std::unordered_map<std::string, std::string>& HTMLParserContext::getResolvedStyle(
    const std::shared_ptr<DOMNode>& node) {
  auto it = _stylePropertiesCache.find(node.get());
  if (it != _stylePropertiesCache.end()) {
    return it->second;
  }
  auto& slot = _stylePropertiesCache[node.get()];

  // Priority: element defaults -> element rules from <style> -> class rules -> inline.
  const auto& tagDefaults = ElementDefaults();
  auto tagIt = tagDefaults.find(node->name);
  if (tagIt != tagDefaults.end()) {
    ParseStyleString(tagIt->second, slot);
  }
  auto elemRuleIt = _cssElementRules.find(node->name);
  if (elemRuleIt != _cssElementRules.end()) {
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

std::string HTMLParserContext::getStyleProperty(const std::shared_ptr<DOMNode>& node,
                                                const std::string& property,
                                                const std::string& fallback) {
  const auto& props = getResolvedStyle(node);
  auto it = props.find(property);
  if (it != props.end()) return it->second;
  return fallback;
}

HTMLInheritedStyle HTMLParserContext::computeInherited(const std::shared_ptr<DOMNode>& element,
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
        warn("html: font-family generic '" + token + "' not mapped on this platform; dropped");
        continue;
      }
      out.fontFamilyChain.push_back(resolved);
    }
    if (!out.fontFamilyChain.empty()) {
      out.primaryFontFamily = out.fontFamilyChain.front();
    }
    recordFontFallbacks(out.fontFamilyChain);
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
  // Compute combined font-style label used by PAGX Text. Maps CSS font-weight (numeric or
  // keyword) and font-style to a single PAGX `fontStyle` keyword such as "Bold Italic" or
  // "Black Italic". Regular weight collapses the weight portion to empty.
  out.fontStyleName = ResolveFontStyleName(out.fontWeight, out.fontStyle);

  static const char* TextDisallowed[] = {
      "text-transform", "text-indent",  "word-spacing", "unicode-bidi",
      "font-variant",   "font-stretch", "font"};
  for (const auto* prop : TextDisallowed) {
    if (!LookupProperty(props, prop).empty()) {
      warn(std::string("html: ") + prop + " not supported; ignored");
    }
  }
  std::string textOverflow = LookupLowerTrimmed(props, "text-overflow");
  if (textOverflow == "ellipsis") {
    warn("html: text-overflow: ellipsis is not implemented in PAGX");
  }

  // Refresh the pre-resolved numeric forms so text-leaf conversion can read them directly
  // without re-parsing strings for every fragment.
  if (out.fontSize.empty()) {
    out.fontSizePx = HTML_DEFAULT_FONT_SIZE;
  } else {
    float fontSizePx = parsePxLength(out.fontSize);
    if (std::isnan(fontSizePx) || fontSizePx <= 0) fontSizePx = HTML_DEFAULT_FONT_SIZE;
    out.fontSizePx = fontSizePx;
  }
  if (out.letterSpacing.empty()) {
    out.letterSpacingPx = 0.0f;
  } else {
    float ls = parsePxLength(out.letterSpacing);
    out.letterSpacingPx = std::isnan(ls) ? 0.0f : ls;
  }
  out.resolvedTextColor =
      parseColor(out.color.empty() ? std::string(HTML_DEFAULT_TEXT_COLOR) : out.color);
  return out;
}

HTMLBoxAttributes HTMLParserContext::resolveBox(const std::shared_ptr<DOMNode>& element) {
  HTMLBoxAttributes box = {};
  const auto& props = getResolvedStyle(element);
  parseBoxSizing(box, props);
  parseBoxPositioning(box, props);
  parseBoxLayout(box, props);
  parseBoxVisuals(box, props);
  parseBoxTransform(box, props);
  return box;
}

void HTMLParserContext::parseBoxSizing(HTMLBoxAttributes& box,
                                       const std::unordered_map<std::string, std::string>& props) {
  ParseSizingDimension(LookupProperty(props, "width"), box.widthPx, box.widthPct);
  ParseSizingDimension(LookupProperty(props, "height"), box.heightPx, box.heightPct);
  std::string boxSizing = LookupLowerTrimmed(props, "box-sizing");
  if (!boxSizing.empty() && boxSizing != "border-box") {
    warn("html: box-sizing: " + boxSizing + " not supported; treated as border-box");
  }
  static const char* SizingDisallowed[] = {"min-width", "min-height", "max-width", "max-height",
                                           "aspect-ratio"};
  for (const auto* prop : SizingDisallowed) {
    if (!LookupProperty(props, prop).empty()) {
      warn(std::string("html: ") + prop + " not supported; ignored");
    }
  }
}

void HTMLParserContext::parseBoxPositioning(
    HTMLBoxAttributes& box, const std::unordered_map<std::string, std::string>& props) {
  std::string pos = LookupLowerTrimmed(props, "position");
  if (pos == "absolute") {
    box.absolute = true;
  } else if (!pos.empty() && pos != "static") {
    warn("html: position: " + pos + " not supported; downgraded to absolute");
    box.absolute = true;
  }
  if (!box.absolute) return;
  const std::string& left = LookupProperty(props, "left");
  if (!left.empty()) box.leftPx = parsePxLength(left);
  const std::string& right = LookupProperty(props, "right");
  if (!right.empty()) box.rightPx = parsePxLength(right);
  const std::string& top = LookupProperty(props, "top");
  if (!top.empty()) box.topPx = parsePxLength(top);
  const std::string& bottom = LookupProperty(props, "bottom");
  if (!bottom.empty()) box.bottomPx = parsePxLength(bottom);
}

void HTMLParserContext::parseBoxLayout(HTMLBoxAttributes& box,
                                       const std::unordered_map<std::string, std::string>& props) {
  std::string disp = LookupLowerTrimmed(props, "display");
  if (disp == "flex") {
    box.displayFlex = true;
  } else if (!disp.empty() && disp != "block" && disp != "inline" && disp != "inline-block") {
    warn("html: display: " + disp + " not supported; ignored");
  } else if (disp == "inline-block") {
    warn("html: display: inline-block not supported; treated as block");
  }
  std::string fd = LookupLowerTrimmed(props, "flex-direction");
  if (fd == "column" || fd == "column-reverse") {
    box.flexRow = false;
    if (fd == "column-reverse") warn("html: flex-direction: column-reverse not supported");
  } else if (fd == "row-reverse") {
    warn("html: flex-direction: row-reverse not supported");
  }
  const std::string& gap = LookupProperty(props, "gap");
  if (!gap.empty()) {
    box.gapPx = parsePxLength(gap);
    box.gapSet = !std::isnan(box.gapPx);
    if (!box.gapSet) box.gapPx = 0;
  }
  const std::string& padding = LookupProperty(props, "padding");
  if (!padding.empty()) {
    auto tokens = SplitTopLevelWhitespace(padding);
    std::vector<float> nums;
    for (auto& t : tokens) {
      float v = parsePxLength(t);
      if (std::isnan(v)) {
        warn("html: invalid padding token '" + t + "'");
        continue;
      }
      nums.push_back(v);
    }
    box.padding = BuildPaddingShorthand(nums);
    box.paddingSet = !nums.empty();
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
      warn("html: flex shorthand '" + flex + "' not supported beyond 'flex: N'");
    }
  }
  if (!LookupProperty(props, "flex-wrap").empty()) {
    warn("html: flex-wrap not supported; ignored");
  }
  static const char* LayoutDisallowed[] = {"flex-grow", "flex-shrink",   "flex-basis", "float",
                                           "order",     "align-content", "align-self", "direction"};
  for (const auto* prop : LayoutDisallowed) {
    if (!LookupProperty(props, prop).empty()) {
      warn(std::string("html: ") + prop + " not supported; ignored");
    }
  }
  if (!LookupProperty(props, "margin").empty()) {
    warn("html: margin not supported; use padding/gap/flex");
  }
  if (!LookupProperty(props, "margin-top").empty()) warn("html: margin-top not supported");
  if (!LookupProperty(props, "margin-right").empty()) warn("html: margin-right not supported");
  if (!LookupProperty(props, "margin-bottom").empty()) warn("html: margin-bottom not supported");
  if (!LookupProperty(props, "margin-left").empty()) warn("html: margin-left not supported");
  if (!LookupProperty(props, "grid-template-columns").empty()) {
    warn("html: grid layout not supported");
  }
  if (!LookupProperty(props, "grid-template-rows").empty()) {
    warn("html: grid layout not supported");
  }
}

void HTMLParserContext::parseBoxVisuals(HTMLBoxAttributes& box,
                                        const std::unordered_map<std::string, std::string>& props) {
  std::string bgColor = LookupProperty(props, "background-color");
  if (bgColor.empty()) {
    bgColor = LookupProperty(props, "background");  // accept shorthand if it's color-only
  }
  // `parseColor` accepts hex, named colors, and `rgb()/rgba()` literals. We only need to bail
  // out when the value is actually a non-color shorthand (gradient / url-image).
  if (!bgColor.empty() && bgColor.find("gradient") == std::string::npos &&
      bgColor.find("url(") == std::string::npos) {
    box.backgroundColor = parseColor(bgColor);
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
  // `background-clip: text` is the only clip value the importer models. The subset transformer
  // already normalises every other keyword to empty, so a non-empty value here equals `text`.
  std::string bgClip = LookupLowerTrimmed(props, "background-clip");
  if (bgClip == "text") {
    box.backgroundClipText = true;
  }

  const std::string& br = LookupProperty(props, "border-radius");
  if (!br.empty()) {
    // CSS `border-radius` shorthand accepts up to 4 lengths or percentages (top-left,
    // top-right, bottom-right, bottom-left), with an optional '/' separating horizontal
    // and vertical radii for elliptical corners. PAGX `Rectangle` exposes a single
    // uniform `roundness`, so:
    //   - elliptical form (containing '/'): warn and drop the value;
    //   - asymmetric corner values: warn and approximate with the largest radius
    //     so the corners that should be rounded stay rounded (corners that should
    //     be square become rounded, but losing all rounding is the worse failure);
    //   - percentage tokens (e.g. `50%` for the classic pill / circle pattern) are
    //     resolved against the box's known px dimensions. Because PAGX has only a
    //     uniform corner radius, we resolve against `min(widthPx, heightPx)` so a
    //     square element with `50%` produces a true circle. Percentages on elements
    //     without a fixed px size are dropped with a warning.
    if (br.find('/') != std::string::npos) {
      warn("html: elliptical border-radius '" + br + "' not supported by PAGX Rectangle; ignored");
    } else {
      auto tokens = SplitTopLevelWhitespace(br);
      std::vector<float> nums;
      for (auto& t : tokens) {
        std::string trimmed = Trim(t);
        float v = NAN;
        if (!trimmed.empty() && trimmed.back() == '%') {
          char* end = nullptr;
          float pct = std::strtof(trimmed.c_str(), &end);
          if (end == trimmed.c_str()) {
            warn("html: invalid border-radius token '" + t + "'");
            continue;
          }
          if (std::isnan(box.widthPx) || std::isnan(box.heightPx)) {
            warn("html: percentage border-radius '" + t +
                 "' requires fixed px width/height on the same element; ignored");
            continue;
          }
          v = pct * std::min(box.widthPx, box.heightPx) / 100.0f;
        } else {
          v = parsePxLength(trimmed);
          if (std::isnan(v)) {
            warn("html: invalid border-radius token '" + t + "'");
            continue;
          }
        }
        nums.push_back(v);
      }
      if (!nums.empty()) {
        float maxR = nums[0];
        bool asymmetric = false;
        for (size_t i = 1; i < nums.size(); ++i) {
          if (nums[i] != nums[0]) asymmetric = true;
          if (nums[i] > maxR) maxR = nums[i];
        }
        if (asymmetric) {
          warn("html: per-corner border-radius '" + br +
               "' approximated with a single roundness (PAGX Rectangle does not "
               "support per-corner radii)");
        }
        box.borderRadiusPx = maxR;
        box.borderRadiusSet = true;
      }
    }
  }

  const std::string& border = LookupProperty(props, "border");
  if (!border.empty()) {
    auto tokens = SplitTopLevelWhitespace(border);
    // Collect candidate colour tokens — anything that's not a width / known style keyword.
    // Take the FIRST candidate as the border colour and warn on any subsequent ones so the
    // author notices `border: 1px solid red blue`-style mistakes (previously the second
    // colour silently overwrote the first).
    std::string colorToken;
    bool sawExtraColor = false;
    for (auto& t : tokens) {
      float w = parsePxLength(t);
      if (!std::isnan(w)) {
        box.borderWidthPx = w;
        continue;
      }
      std::string lt = ToLower(t);
      if (lt == "solid" || lt == "none") continue;
      if (lt == "dashed" || lt == "dotted" || lt == "double" || lt == "groove" || lt == "ridge" ||
          lt == "inset" || lt == "outset") {
        warn("html: border style '" + lt + "' not supported; treated as solid");
        continue;
      }
      if (colorToken.empty()) {
        colorToken = t;
      } else {
        sawExtraColor = true;
      }
    }
    if (sawExtraColor) {
      warn("html: border value '" + border + "' has multiple colour tokens; first one used");
    }
    if (!colorToken.empty()) {
      box.borderColor = parseColor(colorToken);
    }
    box.borderSet = box.borderWidthPx > 0;
  }

  box.boxShadow = LookupProperty(props, "box-shadow");
  box.filter = LookupProperty(props, "filter");
  box.backdropFilter = LookupProperty(props, "backdrop-filter");

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
    warn("html: overflow: " + overflow + " not fully supported");
  }

  box.objectFit = LookupLowerTrimmed(props, "object-fit");

  static const char* VisualsDisallowed[] = {"background-size",     "background-repeat",
                                            "background-position", "outline",
                                            "perspective",         "clip-path"};
  for (const auto* prop : VisualsDisallowed) {
    if (!LookupProperty(props, prop).empty()) {
      warn(std::string("html: ") + prop + " not supported; ignored");
    }
  }
}

void HTMLParserContext::parseBoxTransform(
    HTMLBoxAttributes& box, const std::unordered_map<std::string, std::string>& props) {
  // The current importer only honours `transform-origin: 50% 50%` (the CSS default), which
  // matches the inline output emitted by the html-snapshot tool. Anything else would require
  // the resolver to translate the origin into a TextBox `anchor` offset. Warn so authors of
  // hand-written subset HTML notice — this used to live at the tail of this function and
  // was therefore skipped on every transform-parsing failure (`return` early), masking the
  // real configuration issue.
  std::string origin = ToLower(Trim(LookupProperty(props, "transform-origin")));
  if (!origin.empty() && origin != "50% 50%" && origin != "center" && origin != "center center") {
    warn("html: transform-origin '" + origin + "' is not supported; assuming default '50% 50%'");
  }

  std::string transform = Trim(LookupProperty(props, "transform"));
  if (transform.empty()) return;

  std::string fn;
  std::string args;
  if (!SplitTransformFunction(transform, fn, args)) {
    warn("html: transform '" + transform + "' is malformed; ignored");
    return;
  }
  auto parts = SplitCommaArgs(args);
  HTMLTransform parsed = {};

  if (fn == "skewx") {
    if (parts.size() != 1) {
      warn("html: skewX expects 1 argument; got '" + transform + "'");
      return;
    }
    parsed.skew = -ParseAngle(parts[0]);
    parsed.skewAxis = 0.0f;
    parsed.valid = true;
  } else if (fn == "skewy") {
    if (parts.size() != 1) {
      warn("html: skewY expects 1 argument; got '" + transform + "'");
      return;
    }
    parsed.skew = ParseAngle(parts[0]);
    parsed.skewAxis = 90.0f;
    parsed.valid = true;
  } else if (fn == "rotate") {
    if (parts.size() != 1) {
      warn("html: rotate expects 1 argument; got '" + transform + "'");
      return;
    }
    parsed.rotation = ParseAngle(parts[0]);
    parsed.valid = true;
  } else if (fn == "scale") {
    float sx = 1.0f;
    float sy = 1.0f;
    if (parts.size() == 1 && ParseScalarFloat(parts[0], sx)) {
      sy = sx;
    } else if (parts.size() == 2 && ParseScalarFloat(parts[0], sx) &&
               ParseScalarFloat(parts[1], sy)) {
      // sx/sy already filled.
    } else {
      warn("html: scale expects 1 or 2 numeric arguments; got '" + transform + "'");
      return;
    }
    parsed.scaleX = sx;
    parsed.scaleY = sy;
    parsed.valid = true;
  } else if (fn == "scalex" || fn == "scaley") {
    float v = 1.0f;
    if (parts.size() != 1 || !ParseScalarFloat(parts[0], v)) {
      warn("html: " + fn + " expects 1 numeric argument; got '" + transform + "'");
      return;
    }
    if (fn == "scalex") {
      parsed.scaleX = v;
    } else {
      parsed.scaleY = v;
    }
    parsed.valid = true;
  } else if (fn == "translate") {
    if (parts.empty() || parts.size() > 2) {
      warn("html: translate expects 1 or 2 length arguments; got '" + transform + "'");
      return;
    }
    float tx = parsePxLength(parts[0]);
    if (std::isnan(tx)) {
      warn("html: translate first argument '" + parts[0] + "' is not a px length; ignored");
      return;
    }
    float ty = 0.0f;
    if (parts.size() == 2) {
      ty = parsePxLength(parts[1]);
      if (std::isnan(ty)) {
        warn("html: translate second argument '" + parts[1] + "' is not a px length; ignored");
        return;
      }
    }
    parsed.translateX = tx;
    parsed.translateY = ty;
    parsed.valid = true;
  } else if (fn == "translatex" || fn == "translatey") {
    if (parts.size() != 1) {
      warn("html: " + fn + " expects 1 length argument; got '" + transform + "'");
      return;
    }
    float v = parsePxLength(parts[0]);
    if (std::isnan(v)) {
      warn("html: " + fn + " argument '" + parts[0] + "' is not a px length; ignored");
      return;
    }
    if (fn == "translatex") {
      parsed.translateX = v;
    } else {
      parsed.translateY = v;
    }
    parsed.valid = true;
  } else {
    warn("html: transform function '" + fn + "' is not in the supported subset");
    return;
  }

  box.transform = parsed;
}

}  // namespace pagx
