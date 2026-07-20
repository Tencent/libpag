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

#include "pagx/html/importer/HTMLTransformPasses.h"
// TODO: continue splitting this translation unit per pass (DocumentSkeletonPass.cpp,
// MarginToGapPromotionPass.cpp, …). `HTMLFlexInferencePass` and the shared length /
// margin helpers (`HTMLTransformPassUtils`) have already been hoisted out; the remaining six
// passes still share file-local anonymous-namespace helpers and will move once the surrounding
// API has stabilised. Tracked separately to avoid expanding this PR further.
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "pagx/html/importer/HTMLBoxAttributes.h"
#include "pagx/html/importer/HTMLCssCascade.h"
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLSubsetPropertyTable.h"
#include "pagx/html/importer/HTMLTransformPassUtils.h"

namespace pagx::html {

namespace {

// Float equality with explicit tolerance, used by the margin-to-gap promotion below to compare
// CSS lengths after arithmetic (rounding errors accumulate over `ResolveLength`).
bool ApproxEqual(float a, float b, float eps) {
  return std::fabs(a - b) <= eps;
}

// CSS properties that inherit by default in the subset. Mirrors `HTMLInheritedStyle` in the
// importer so that the cascade carries identical semantics through both layers.
const std::vector<std::string>& InheritableProperties() {
  static const std::vector<std::string> v = {
      "color",
      "font-family",
      "font-size",
      "font-weight",
      "font-style",
      "letter-spacing",
      "line-height",
      "text-align",
      "text-decoration",
      "text-decoration-color",
      "white-space",
      "-webkit-text-stroke",
      "-webkit-text-stroke-width",
      "-webkit-text-stroke-color",
  };
  return v;
}

// Tags that participate in the document skeleton but never produce visual output. The cascade,
// property filter, and style emitter all skip them.
bool IsNonRenderedTag(const std::string& tag) {
  return tag == "html" || tag == "head" || tag == "title" || tag == "meta" || tag == "style";
}

bool IsAllowedTag(const std::string& tag) {
  return IsContainerTag(tag) || IsTextLeafTag(tag) || tag == "br" || tag == "img" || tag == "svg" ||
         tag == "html" || tag == "head" || tag == "body" || tag == "title" || tag == "meta" ||
         tag == "style";
}

// Strip non-element DOM nodes (comments, PIs, stray text-between-elements) from a parent.
// Text nodes whose content is meaningful for the parent (e.g. inside a <p>) are preserved.
void RemoveNonElementChildren(const std::shared_ptr<DOMNode>& parent) {
  std::shared_ptr<DOMNode> prev = nullptr;
  auto child = parent->firstChild;
  while (child) {
    auto next = child->nextSibling;
    if (child->type != DOMNodeType::Element) {
      UnlinkChild(parent, prev, child);
    } else {
      prev = child;
    }
    child = next;
  }
}

std::shared_ptr<DOMNode> MakeElement(const std::string& name) {
  auto node = std::make_shared<DOMNode>();
  node->name = name;
  node->type = DOMNodeType::Element;
  return node;
}

}  // namespace

//==================================================================================================
// DocumentSkeletonPass
//==================================================================================================

namespace {

// Merge `dup`'s children onto the end of `first`'s child list, then unlink `dup` from
// `parent`'s child list. `prev` is `dup`'s predecessor (or null when `dup` was the first
// child). Used by the head / body deduplication path so the two cases share one
// implementation.
void AbsorbDuplicateInto(const std::shared_ptr<DOMNode>& parent,
                         const std::shared_ptr<DOMNode>& first, const std::shared_ptr<DOMNode>& dup,
                         const std::shared_ptr<DOMNode>& prev) {
  auto sub = dup->firstChild;
  std::shared_ptr<DOMNode> last = first->firstChild;
  while (last && last->nextSibling) last = last->nextSibling;
  if (!first->firstChild) {
    first->firstChild = sub;
  } else if (last) {
    last->nextSibling = sub;
  }
  UnlinkChild(parent, prev, dup);
  dup->firstChild = nullptr;
}

}  // namespace

void DocumentSkeletonPass::apply(const std::shared_ptr<DOMNode>& root, HTMLTransformContext& ctx) {
  if (!root) {
    ctx.error("subset:no-root", "html: input has no DOM root");
    return;
  }
  LowercaseTagsInPlace(root);
  if (root->name != "html") {
    ctx.error("subset:no-html-root",
              "html: expected <html> as document root, got <" + root->name + ">", root);
    return;
  }

  // Strip non-element children at root (PIs, comments, stray text).
  RemoveNonElementChildren(root);

  // Locate <head> and <body>; merge duplicates.
  std::shared_ptr<DOMNode> head = nullptr;
  std::shared_ptr<DOMNode> body = nullptr;
  std::shared_ptr<DOMNode> prev = nullptr;
  auto child = root->firstChild;
  while (child) {
    auto next = child->nextSibling;
    if (child->name == "head") {
      if (!head) {
        head = child;
        prev = child;
      } else {
        AbsorbDuplicateInto(root, head, child, prev);
      }
    } else if (child->name == "body") {
      if (!body) {
        body = child;
        prev = child;
      } else {
        AbsorbDuplicateInto(root, body, child, prev);
      }
    } else {
      // Stray top-level element. The subset doesn't permit anything between <html> and
      // <head>/<body>; drop the offender with the same diagnostic the legacy importer used so
      // downstream tooling (and existing tests) keep matching on the message substring.
      ctx.warn("subset:unsupported-tag",
               "html: element '" + child->name + "' at root is not allowed; skipped", child);
      UnlinkChild(root, prev, child);
    }
    child = next;
  }

  if (!body) {
    ctx.error("subset:no-body", "html: input has no <body> element", root);
    return;
  }

  // Re-link children of root in canonical order: head (when present) then body.
  root->firstChild = nullptr;
  std::shared_ptr<DOMNode>* tail = &root->firstChild;
  if (head) {
    *tail = head;
    head->nextSibling = nullptr;
    tail = &head->nextSibling;
  }
  *tail = body;
  body->nextSibling = nullptr;

  // Inside <head>, keep only <title>, <meta>, <style>.
  if (head) {
    RemoveNonElementChildren(head);
    std::shared_ptr<DOMNode> prevHeadChild = nullptr;
    auto headChild = head->firstChild;
    while (headChild) {
      auto next = headChild->nextSibling;
      bool drop = false;
      if (headChild->name == "title" || headChild->name == "meta" || headChild->name == "style") {
        // keep
      } else if (headChild->name == "link") {
        auto* rel = headChild->findAttribute("rel");
        if (rel && ToLower(Trim(*rel)) == "stylesheet") {
          ctx.warn("subset:external-stylesheet", "html: external <link rel=\"stylesheet\"> dropped",
                   headChild);
        }
        drop = true;
      } else {
        ctx.warn("subset:unsupported-tag",
                 "html: <" + headChild->name + "> inside <head> is not allowed; dropped",
                 headChild);
        drop = true;
      }
      if (drop) {
        UnlinkChild(head, prevHeadChild, headChild);
      } else {
        prevHeadChild = headChild;
      }
      headChild = next;
    }
  }
}

//==================================================================================================
// StyleSheetCollectorPass
//==================================================================================================

void StyleSheetCollectorPass::apply(const std::shared_ptr<DOMNode>& root,
                                    HTMLTransformContext& ctx) {
  if (!root || ctx.hasFatal()) return;
  auto head = root->getFirstChild("head");
  if (!head) return;

  std::string concatenated;
  auto child = head->firstChild;
  while (child) {
    auto next = child->nextSibling;
    if (child->type == DOMNodeType::Element && child->name == "style") {
      auto textChild = child->firstChild;
      if (textChild && textChild->type == DOMNodeType::Text) {
        if (!concatenated.empty()) concatenated.push_back('\n');
        concatenated += textChild->name;
      }
    }
    child = next;
  }
  if (!concatenated.empty()) {
    std::vector<std::string> droppedAt;
    auto rules = TokenizeStyleSheet(concatenated, droppedAt);
    for (auto& dropped : droppedAt) {
      ctx.warn("subset:unsupported-at-rule",
               "html: at-rule '" + dropped + "' dropped (not in subset)", nullptr);
    }
    for (auto& rule : rules) {
      auto selectors = SplitTopLevelCommas(rule.selectors);
      PropertyMap decls;
      ParseStyleString(rule.declarations, decls);
      if (decls.empty()) continue;
      for (auto& selRaw : selectors) {
        std::string sel = Trim(selRaw);
        if (sel.empty()) continue;
        auto parsed = ClassifySelector(sel);
        switch (parsed.kind) {
          case SelectorKind::Class:
            ctx.addClassRule(parsed.key, decls);
            break;
          case SelectorKind::Element:
            ctx.addElementRule(parsed.key, decls);
            break;
          case SelectorKind::Universal:
            ctx.warn("subset:unsupported-selector",
                     "html: universal selector '*' is not allowed; rule dropped", nullptr);
            break;
          case SelectorKind::Unsupported:
            ctx.warn("subset:unsupported-selector",
                     "html: selector '" + sel + "' is not supported; rule dropped", nullptr);
            break;
        }
      }
    }
  }

  // Remove <style> elements unless preservation is requested.
  if (!ctx.options().preserveStyleBlock) {
    std::shared_ptr<DOMNode> prev = nullptr;
    auto child = head->firstChild;
    while (child) {
      auto next = child->nextSibling;
      if (child->type == DOMNodeType::Element && child->name == "style") {
        UnlinkChild(head, prev, child);
      } else {
        prev = child;
      }
      child = next;
    }
  }
}

//==================================================================================================
// ComputedStylePass
//==================================================================================================

namespace {

void ResolveElement(const std::shared_ptr<DOMNode>& element, const PropertyMap& inherited,
                    HTMLTransformContext& ctx);

// Moves a `-webkit-`-prefixed declaration onto its standard counterpart when the standard one
// is absent, then erases the prefixed entry. Used during ComputedStylePass so the subset
// property filter only has to register the canonical name.
void CoalesceWebkitAlias(PropertyMap& resolved, const std::string& vendorName,
                         const std::string& standardName) {
  auto vendorIt = resolved.find(vendorName);
  if (vendorIt == resolved.end()) return;
  resolved.emplace(standardName, std::move(vendorIt->second));
  resolved.erase(vendorIt);
}

// ----- `background` shorthand decomposition -------------------------------------------------

// A token that names a `background-image` source: `url(...)` or any `*-gradient(...)`.
bool IsBackgroundImageToken(const std::string& lower) {
  return lower.rfind("url(", 0) == 0 || lower.find("gradient(") != std::string::npos;
}

// A `background-repeat` keyword.
bool IsBackgroundRepeatToken(const std::string& lower) {
  return lower == "repeat" || lower == "no-repeat" || lower == "repeat-x" || lower == "repeat-y" ||
         lower == "space" || lower == "round";
}

// `background-attachment` / `background-origin` / `background-clip` box keywords. PAGX has no
// analogue for these, so they are recognised only to skip them during decomposition. (The
// `background-clip: text` gradient-text technique is written as a separate longhand, never inside
// the `background` shorthand, so it is handled elsewhere and not listed here.)
bool IsBackgroundBoxOrAttachmentToken(const std::string& lower) {
  return lower == "scroll" || lower == "fixed" || lower == "local" || lower == "border-box" ||
         lower == "padding-box" || lower == "content-box";
}

// Heuristically recognises a CSS <color> token: hex, a functional colour notation, the CSS-wide
// `currentcolor`, or a named colour from the CSS table (which also covers `transparent`).
bool IsColorToken(const std::string& token) {
  std::string lower = ToLower(Trim(token));
  if (lower.empty()) return false;
  if (lower[0] == '#') return true;
  static const char* kColorFns[] = {"rgb(", "rgba(", "hsl(",   "hsla(",  "hwb(",
                                    "lab(", "lch(",  "oklab(", "oklch(", "color("};
  for (const char* fn : kColorFns) {
    if (lower.rfind(fn, 0) == 0) return true;
  }
  if (lower == "currentcolor") return true;
  const auto& named = NamedColors();
  return named.find(lower) != named.end();
}

// Splits one `background` layer into whitespace tokens while promoting a top-level `/` (which
// separates `<position> / <size>`) to its own token even when written without surrounding
// spaces (`center/cover`). Slashes inside `url(...)` / `gradient(...)` are left untouched.
std::vector<std::string> SplitBackgroundLayerTokens(const std::string& layer) {
  std::string spaced;
  spaced.reserve(layer.size() + 8);
  int depth = 0;
  for (char c : layer) {
    if (c == '(') {
      depth++;
    } else if (c == ')' && depth > 0) {
      depth--;
    }
    if (c == '/' && depth == 0) {
      spaced += " / ";
    } else {
      spaced += c;
    }
  }
  return SplitTopLevelWhitespace(spaced);
}

std::string JoinTokens(const std::vector<std::string>& tokens) {
  std::string out;
  for (const auto& t : tokens) {
    if (!out.empty()) out += ' ';
    out += t;
  }
  return out;
}

// Expands the `background` shorthand into the longhand properties the subset property table
// understands (`background-color`, `background-image`, `background-position`, `background-size`,
// `background-repeat`). Without this the shorthand is not in the table and `PropertyFilterPass`
// drops it, silently losing the box's fill. This mirrors the computed-style expansion the
// html-snapshot browser pass performs, so raw HTML fed straight through the importer paints
// identically — including combined forms such as `background:#fff url(x) center/cover no-repeat`
// and stacked gradients. Only the final comma-separated layer may carry the colour. An explicit
// longhand already present wins (`emplace` never overwrites), so a separate `background-color`
// declaration is preserved.
void ExpandBackgroundShorthand(PropertyMap& resolved) {
  auto it = resolved.find("background");
  if (it == resolved.end()) return;
  std::string value = Trim(it->second);
  resolved.erase(it);
  if (value.empty()) return;

  auto layers = SplitTopLevelCommas(value);
  std::vector<std::string> imageLayers;
  std::string colorToken;
  std::string positionStr;
  std::string sizeStr;
  std::string repeatStr;

  for (size_t li = 0; li < layers.size(); ++li) {
    const bool isLast = (li + 1 == layers.size());
    std::string imageToken;
    std::vector<std::string> posTokens;
    std::vector<std::string> sizeTokens;
    bool afterSlash = false;
    for (const auto& tok : SplitBackgroundLayerTokens(layers[li])) {
      if (tok == "/") {
        afterSlash = true;
        continue;
      }
      std::string lower = ToLower(tok);
      if (IsBackgroundImageToken(lower)) {
        imageToken = tok;
      } else if (lower == "none") {
        // `background-image: none`; nothing to paint from this layer's image slot.
      } else if (isLast && colorToken.empty() && IsColorToken(tok)) {
        colorToken = tok;
      } else if (IsBackgroundRepeatToken(lower)) {
        repeatStr = lower;
      } else if (IsBackgroundBoxOrAttachmentToken(lower)) {
        // No PAGX analogue — skip.
      } else if (afterSlash) {
        sizeTokens.push_back(tok);
      } else {
        posTokens.push_back(tok);
      }
    }
    if (!imageToken.empty()) {
      imageLayers.push_back(std::move(imageToken));
      // Position / size apply to this layer's image; keep the last image layer's values (the
      // importer only consumes them for single `url(...)` backgrounds).
      if (!posTokens.empty()) positionStr = JoinTokens(posTokens);
      if (!sizeTokens.empty()) sizeStr = JoinTokens(sizeTokens);
    }
  }

  if (!imageLayers.empty()) {
    std::string joined;
    for (const auto& layer : imageLayers) {
      if (!joined.empty()) joined += ", ";
      joined += layer;
    }
    resolved.emplace("background-image", std::move(joined));
    if (!positionStr.empty()) resolved.emplace("background-position", positionStr);
    if (!sizeStr.empty()) resolved.emplace("background-size", sizeStr);
    if (!repeatStr.empty()) resolved.emplace("background-repeat", repeatStr);
  }
  if (!colorToken.empty()) {
    resolved.emplace("background-color", colorToken);
  }
}

void ResolveTree(const std::shared_ptr<DOMNode>& element, const PropertyMap& inheritedIn,
                 HTMLTransformContext& ctx, int depth) {
  if (ShouldSkipWalkerNode(element, depth, ctx, "style resolution")) return;
  PropertyMap inherited = inheritedIn;
  // Compute this element's resolved style and update `inherited` for descendants.
  if (!IsNonRenderedTag(element->name)) {
    ResolveElement(element, inheritedIn, ctx);
    auto* resolved = ctx.findResolved(element.get());
    if (resolved) {
      for (const auto& key : InheritableProperties()) {
        auto it = resolved->find(key);
        if (it != resolved->end() && !it->second.empty()) {
          inherited[key] = it->second;
        }
      }
    }
  }
  // SVG subtrees are opaque to the cascade.
  if (IsOpaqueSubtreeRoot(element)) return;
  auto child = element->firstChild;
  while (child) {
    ResolveTree(child, inherited, ctx, depth + 1);
    child = child->nextSibling;
  }
}

void ResolveElement(const std::shared_ptr<DOMNode>& element, const PropertyMap& inherited,
                    HTMLTransformContext& ctx) {
  PropertyMap resolved;
  // 1. Inherited (only inheritable properties).
  for (const auto& kv : inherited) {
    resolved[kv.first] = kv.second;
  }
  // 2. Element defaults.
  const auto& defaults = ParsedElementDefaults();
  auto defaultIt = defaults.find(element->name);
  if (defaultIt != defaults.end()) {
    for (const auto& kv : defaultIt->second) {
      resolved[kv.first] = kv.second;
    }
  }
  // 3. Element rules from <style> block.
  auto elemRuleIt = ctx.elementRules().find(element->name);
  if (elemRuleIt != ctx.elementRules().end()) {
    for (const auto& kv : elemRuleIt->second) {
      resolved[kv.first] = kv.second;
    }
  }
  // 4. Class rules (in declaration order).
  auto* classAttr = element->findAttribute("class");
  if (classAttr && !classAttr->empty()) {
    const auto& s = *classAttr;
    size_t p = 0;
    while (p < s.size()) {
      while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) p++;
      if (p >= s.size()) break;
      size_t e = p;
      while (e < s.size() && !std::isspace(static_cast<unsigned char>(s[e]))) e++;
      auto classIt = ctx.classRules().find(s.substr(p, e - p));
      if (classIt != ctx.classRules().end()) {
        for (const auto& kv : classIt->second) {
          resolved[kv.first] = kv.second;
        }
      }
      p = e;
    }
  }
  // 5. Inline style="…" (highest priority).
  auto* styleAttr = element->findAttribute("style");
  if (styleAttr && !styleAttr->empty()) {
    ParseStyleString(*styleAttr, resolved);
  }
  // Coalesce vendor aliases onto their standard names so the subset property table only
  // needs to register the canonical form. The standard name wins when both are present.
  CoalesceWebkitAlias(resolved, "-webkit-background-clip", "background-clip");
  // Expand the `background` shorthand so `PropertyFilterPass` keeps the paint instead of
  // dropping the (unlisted) shorthand and losing the box's fill.
  ExpandBackgroundShorthand(resolved);
  ctx.setResolved(element.get(), std::move(resolved));
}

}  // namespace

void ComputedStylePass::apply(const std::shared_ptr<DOMNode>& root, HTMLTransformContext& ctx) {
  if (!root || ctx.hasFatal()) return;
  auto body = root->getFirstChild("body");
  if (!body) return;
  PropertyMap empty;
  ResolveTree(body, empty, ctx, 0);
}

//==================================================================================================
// PropertyFilterPass
//==================================================================================================

namespace {

// Parses a font-size value that's already been resolved into px. Falls back to 0 when the
// input still contains a relative unit (`em` / unitless), which should not happen after the
// font-size has been filtered.
float ParseResolvedFontSizePx(const std::string& value) {
  if (value.empty()) return 0.0f;
  char* end = nullptr;
  float v = std::strtof(value.c_str(), &end);
  if (end == value.c_str()) return 0.0f;
  std::string unit = Trim(end);
  if (unit.empty() || unit == "px") return v;
  return 0.0f;
}

// Filters one element. Returns the resolved font-size in pixels (after em/rem resolution) so
// callers can pass it into descendants. `parentFontSizePx` is the parent's resolved font-size,
// used as the base for `em` units appearing in the current element.
float FilterElement(const std::shared_ptr<DOMNode>& element, float parentFontSizePx,
                    HTMLTransformContext& ctx) {
  auto* resolved = ctx.findResolved(element.get());
  if (!resolved) return parentFontSizePx;

  const auto& table = SubsetPropertyTable();
  PropertyMap filtered;

  // Phase 1: resolve `font-size` first using the PARENT's font-size as the em base. CSS spec:
  // `font-size: 2em` resolves against the parent's computed font-size, not the element's own.
  float ownFontSizePx = parentFontSizePx;
  auto fsIt = resolved->find("font-size");
  if (fsIt != resolved->end()) {
    PropertyContext pctx = {};
    pctx.propertyName = "font-size";
    pctx.ownerTag = element->name;
    pctx.canvasWidth = ctx.canvasWidth();
    pctx.canvasHeight = ctx.canvasHeight();
    pctx.currentFontSizePx = parentFontSizePx;
    std::string newValue = ResolveLength(fsIt->second, pctx, ctx);
    if (!newValue.empty()) {
      filtered["font-size"] = newValue;
      float parsed = ParseResolvedFontSizePx(newValue);
      if (parsed > 0.0f) ownFontSizePx = parsed;
    }
  }

  // Phase 2: filter every other property using the just-computed own font-size as the em base.
  for (const auto& kv : *resolved) {
    const std::string& propName = kv.first;
    const std::string& propValue = kv.second;
    if (propName.empty() || propValue.empty()) continue;
    if (propName == "font-size") continue;  // already handled in phase 1
    if (IsDataAttribute(propName)) {
      filtered[propName] = propValue;
      continue;
    }
    auto handlerIt = table.find(propName);
    if (handlerIt == table.end()) {
      ctx.warn("subset:unsupported-property",
               "html: '" + propName + "' is not in the subset; dropped", element);
      continue;
    }
    const auto& handler = handlerIt->second;
    switch (handler.action) {
      case PropAction::Keep:
        filtered[propName] = Trim(propValue);
        break;
      case PropAction::Drop: {
        std::string msg = handler.dropMessage ? handler.dropMessage : "is not in the subset";
        ctx.warn("subset:unsupported-property",
                 "html: '" + propName + ": " + propValue + "' " + msg, element);
        break;
      }
      case PropAction::Transform: {
        PropertyContext pctx = {};
        pctx.propertyName = propName;
        pctx.ownerTag = element->name;
        pctx.canvasWidth = ctx.canvasWidth();
        pctx.canvasHeight = ctx.canvasHeight();
        pctx.currentFontSizePx = ownFontSizePx;
        std::string newValue = handler.transform(propValue, pctx, ctx);
        if (!newValue.empty()) {
          filtered[propName] = newValue;
        }
        break;
      }
    }
  }
  ctx.setResolved(element.get(), std::move(filtered));
  return ownFontSizePx;
}

void FilterTree(const std::shared_ptr<DOMNode>& element, float parentFontSizePx,
                HTMLTransformContext& ctx, int depth) {
  if (ShouldSkipWalkerNode(element, depth, ctx, "property filtering")) return;
  float fontSizeForChildren = parentFontSizePx;
  if (!IsNonRenderedTag(element->name)) {
    fontSizeForChildren = FilterElement(element, parentFontSizePx, ctx);
  }
  // SVG subtrees are opaque to property filtering.
  if (IsOpaqueSubtreeRoot(element)) return;
  auto child = element->firstChild;
  while (child) {
    FilterTree(child, fontSizeForChildren, ctx, depth + 1);
    child = child->nextSibling;
  }
}

}  // namespace

void PropertyFilterPass::apply(const std::shared_ptr<DOMNode>& root, HTMLTransformContext& ctx) {
  if (!root || ctx.hasFatal()) return;
  auto body = root->getFirstChild("body");
  if (!body) return;
  // Root document font-size defaults to 14px (matches HTML_DEFAULT_FONT_SIZE / the body
  // element default in `ElementDefaults`).
  FilterTree(body, 14.0f, ctx, 0);
}

//==================================================================================================
// MarginToGapPromotionPass
//==================================================================================================

namespace {

// Tightly bounded `flex-wrap` test: PAGX layout only models single-line flex, so any wrap
// keyword forces this pass to bail. `nowrap` (the spec default) and an empty string are the
// only safe values.
bool FlexContainerWraps(const PropertyMap& props) {
  std::string fw = LookupResolvedLower(props, "flex-wrap");
  return !fw.empty() && fw != "nowrap";
}

// Conservative parser for the `gap` shorthand on a flex container: returns true when the
// property is absent OR when it parses cleanly as a single px value (we then expose that
// value via `outGap`). Any other shape (`auto`, percentage, two-token row/column gap) is
// treated as "already set, do not touch". Single-token `0`/`0px` is treated as unset.
bool ContainerHasExplicitGap(const PropertyMap& props, float& outGap) {
  outGap = 0.0f;
  const std::string& g = LookupResolved(props, "gap");
  if (g.empty()) return false;
  float px = 0.0f;
  if (!ParseNormalisedPx(g, px)) return true;  // unrecognised → treat as set
  if (px <= 0.0f) return false;
  outGap = px;
  return true;
}

// Inspects a single flex container and, when its in-flow children expose a uniform
// per-edge main-axis margin, lifts that margin onto the container's `gap` and clears the
// per-child margin declarations. Two patterns are recognised:
//
//   - Trailing pattern (the Tailwind-style `mb-[N]` on every item):
//     all in-flow children carry the same `margin-bottom` (or `margin-right` for row flex),
//     except optionally the last one which may be `0`. No child carries a leading margin.
//
//   - Leading pattern (the `mt-[N]` from the second item onwards):
//     the first child carries no `margin-top`, every subsequent child carries the same
//     non-zero `margin-top` (or `margin-left`). No child carries a trailing margin.
//
// In both cases the lifted gap equals the per-child margin, which preserves CSS visuals
// once the children render under PAGX's `gap`-aware vertical/horizontal layout.
//
// Bails out (without modification) when:
//   - the container is not `display: flex`, declares `flex-wrap`, or already has a
//     positive `gap`;
//   - fewer than two in-flow children participate;
//   - any participating child carries `flex` grow (the spec routes free space into the
//     child, so the gap value can no longer be inferred from margins alone);
//   - any margin is non-px (e.g. `auto`, percentages);
//   - margins do not match either pattern.
void TryPromoteMarginToGap(const std::shared_ptr<DOMNode>& parent, HTMLTransformContext& ctx) {
  if (!parent || parent->type != DOMNodeType::Element) return;
  auto* parentResolved = ctx.findResolved(parent.get());
  if (!parentResolved) return;
  if (LookupResolvedLower(*parentResolved, "display") != "flex") return;
  if (FlexContainerWraps(*parentResolved)) return;
  float existingGap = 0.0f;
  if (ContainerHasExplicitGap(*parentResolved, existingGap)) return;

  bool row = LookupResolvedLower(*parentResolved, "flex-direction") != "column";

  std::vector<PropertyMap*> childProps;
  std::vector<float> leadMargins;
  std::vector<float> trailMargins;
  for (auto c = parent->firstChild; c; c = c->nextSibling) {
    if (c->type != DOMNodeType::Element) continue;
    auto* cr = ctx.findResolved(c.get());
    if (!cr) return;
    std::string pos = LookupResolvedLower(*cr, "position");
    if (pos == "absolute" || pos == "fixed") continue;
    if (ChildHasFlexGrow(*cr)) return;
    float lead = 0.0f;
    float trail = 0.0f;
    if (!ResolveChildMainMargin(*cr, row, lead, trail)) return;
    childProps.push_back(cr);
    leadMargins.push_back(lead);
    trailMargins.push_back(trail);
  }
  if (childProps.size() < 2) return;

  const size_t n = childProps.size();
  // `MARGIN_EPS_PX` absorbs the same sub-pixel rounding that `ResolveLength` introduces when
  // converting Tailwind arbitrary values; tighter equality misfires on legitimate uniformity.
  constexpr float MARGIN_EPS_PX = 0.05f;

  // Trailing pattern: all leadMargins must be ~0; trailMargins[0..n-2] uniform > 0; the
  // last trailMargin is 0 or matches the others.
  bool trailingOK = true;
  for (float l : leadMargins) {
    if (!ApproxEqual(l, 0.0f, MARGIN_EPS_PX)) {
      trailingOK = false;
      break;
    }
  }
  if (trailingOK) {
    float g = trailMargins[0];
    if (g <= MARGIN_EPS_PX) {
      trailingOK = false;
    } else {
      for (size_t i = 1; i + 1 < n; ++i) {
        if (!ApproxEqual(trailMargins[i], g, MARGIN_EPS_PX)) {
          trailingOK = false;
          break;
        }
      }
      if (trailingOK) {
        float last = trailMargins[n - 1];
        if (!ApproxEqual(last, 0.0f, MARGIN_EPS_PX) && !ApproxEqual(last, g, MARGIN_EPS_PX)) {
          trailingOK = false;
        }
      }
    }
    if (trailingOK) {
      for (auto* cr : childProps) ClearChildMainMargin(*cr, row);
      (*parentResolved)["gap"] = EmitPx(g);
      ctx.warn("subset:margin-promoted-to-gap",
               "html: <" + parent->name + "> per-child main-axis margin lifted to gap=" +
                   EmitPx(g) + " (" + std::string(row ? "row" : "column") + " flex)",
               parent);
      return;
    }
  }

  // Leading pattern: all trailMargins must be ~0; leadMargins[1..n-1] uniform > 0; first
  // leadMargin is 0 or matches the others.
  bool leadingOK = true;
  for (float t : trailMargins) {
    if (!ApproxEqual(t, 0.0f, MARGIN_EPS_PX)) {
      leadingOK = false;
      break;
    }
  }
  if (leadingOK) {
    float g = leadMargins[1];
    if (g <= MARGIN_EPS_PX) {
      leadingOK = false;
    } else {
      for (size_t i = 2; i < n; ++i) {
        if (!ApproxEqual(leadMargins[i], g, MARGIN_EPS_PX)) {
          leadingOK = false;
          break;
        }
      }
      if (leadingOK) {
        float first = leadMargins[0];
        if (!ApproxEqual(first, 0.0f, MARGIN_EPS_PX) && !ApproxEqual(first, g, MARGIN_EPS_PX)) {
          leadingOK = false;
        }
      }
    }
    if (leadingOK) {
      for (auto* cr : childProps) ClearChildMainMargin(*cr, row);
      (*parentResolved)["gap"] = EmitPx(g);
      ctx.warn("subset:margin-promoted-to-gap",
               "html: <" + parent->name + "> per-child main-axis margin lifted to gap=" +
                   EmitPx(g) + " (" + std::string(row ? "row" : "column") + " flex)",
               parent);
      return;
    }
  }
}

void WalkPromoteMarginToGap(const std::shared_ptr<DOMNode>& node, HTMLTransformContext& ctx,
                            int depth) {
  if (ShouldSkipWalkerNode(node, depth, ctx, "margin-to-gap promotion")) return;
  if (IsOpaqueSubtreeRoot(node)) return;
  TryPromoteMarginToGap(node, ctx);
  auto child = node->firstChild;
  while (child) {
    WalkPromoteMarginToGap(child, ctx, depth + 1);
    child = child->nextSibling;
  }
}

}  // namespace

void MarginToGapPromotionPass::apply(const std::shared_ptr<DOMNode>& root,
                                     HTMLTransformContext& ctx) {
  if (!root || ctx.hasFatal()) return;
  auto body = root->getFirstChild("body");
  if (!body) return;
  WalkPromoteMarginToGap(body, ctx, 0);
}

//==================================================================================================
// SpaceJustifyOverflowCollapsePass
//==================================================================================================

namespace {

// Resolves a child's main-axis size against `props`. Tries plain px first; falls back to a
// percentage of `containerMain` (when finite). Returns false when the value is missing or in a
// shape we cannot resolve numerically (e.g. `auto`, `min-content`).
bool ResolveChildMainSize(const PropertyMap& props, bool row, float containerMain, float& outMain) {
  const std::string& raw = LookupResolved(props, row ? "width" : "height");
  if (raw.empty()) return false;
  float px = 0.0f;
  if (ParseNormalisedPx(raw, px)) {
    outMain = px;
    return true;
  }
  std::string trimmed = Trim(raw);
  if (trimmed.size() >= 2 && trimmed.back() == '%' && std::isfinite(containerMain)) {
    float fraction = NAN;
    if (ParseCssPercentage(trimmed, fraction) && std::isfinite(fraction)) {
      outMain = containerMain * fraction;
      return true;
    }
  }
  return false;
}

// Resolves the parent's content-box main-axis size and the gap between flex items. Returns
// false when the parent lacks an explicit px main-axis size, or padding/gap declarations are
// not plain px (so we cannot reason about overflow).
bool ResolveParentMainGeometry(const PropertyMap& props, bool row, float& outAvailableMain,
                               float& outGap) {
  const std::string& raw = LookupResolved(props, row ? "width" : "height");
  if (raw.empty()) return false;
  float main = 0.0f;
  if (!ParseNormalisedPx(raw, main)) return false;
  if (!std::isfinite(main)) return false;
  float padTop = 0.0f;
  float padRight = 0.0f;
  float padBottom = 0.0f;
  float padLeft = 0.0f;
  if (!ParsePaddingFromResolved(props, padTop, padRight, padBottom, padLeft)) return false;
  outAvailableMain = main - (row ? padLeft + padRight : padTop + padBottom);
  outGap = 0.0f;
  const std::string& gap = LookupResolved(props, "gap");
  if (!gap.empty()) {
    float gpx = 0.0f;
    if (!ParseNormalisedPx(gap, gpx)) return false;
    outGap = gpx;
  }
  return true;
}

// Inspects a single flex container and rewrites `justify-content` to `flex-start` when the
// children overflow the main axis. Conservative: bails out of any container where size data
// is incomplete.
void TryCollapseSpaceJustify(const std::shared_ptr<DOMNode>& parent, HTMLTransformContext& ctx) {
  if (!parent || parent->type != DOMNodeType::Element) return;
  auto* parentResolved = ctx.findResolved(parent.get());
  if (!parentResolved) return;
  if (LookupResolvedLower(*parentResolved, "display") != "flex") return;
  std::string jc = LookupResolvedLower(*parentResolved, "justify-content");
  if (jc != "space-between" && jc != "space-around" && jc != "space-evenly") return;

  bool row = LookupResolvedLower(*parentResolved, "flex-direction") != "column";
  float availableMain = 0.0f;
  float gap = 0.0f;
  if (!ResolveParentMainGeometry(*parentResolved, row, availableMain, gap)) return;
  if (!std::isfinite(availableMain)) return;

  float total = 0.0f;
  size_t inFlowCount = 0;
  for (auto c = parent->firstChild; c; c = c->nextSibling) {
    if (c->type != DOMNodeType::Element) continue;
    auto* childResolved = ctx.findResolved(c.get());
    if (!childResolved) return;
    // Out-of-flow children (position: absolute / fixed) do not participate in the flex line;
    // skip them entirely without aborting the analysis.
    std::string pos = LookupResolvedLower(*childResolved, "position");
    if (pos == "absolute" || pos == "fixed") continue;
    // A grow factor would make the spec route the leftover space into the child rather than the
    // packing gaps, so negative free space is not reachable. Be conservative and skip the
    // entire container.
    if (ChildHasFlexGrow(*childResolved)) return;
    float childMain = 0.0f;
    if (!ResolveChildMainSize(*childResolved, row, availableMain, childMain)) return;
    if (!std::isfinite(childMain)) return;
    // CSS flexbox sizes the line by `outer hypothetical main-size`, which includes the child's
    // main-axis margins. Without this term, a child carrying `margin-bottom: 12px` (or its row
    // counterpart) is treated as smaller than it really is, so an overflowing line slips
    // through the `total <= availableMain` check and the original `justify-content` is kept.
    float leadMargin = 0.0f;
    float trailMargin = 0.0f;
    if (!ResolveChildMainMargin(*childResolved, row, leadMargin, trailMargin)) return;
    total += childMain + leadMargin + trailMargin;
    inFlowCount++;
  }
  if (inFlowCount < 2) return;
  total += gap * static_cast<float>(inFlowCount - 1);

  // 0.5px tolerance absorbs sub-pixel rounding from snapshot output. Anything tighter routinely
  // misfires on legitimate fits.
  if (total <= availableMain + 0.5f) return;

  (*parentResolved)["justify-content"] = "flex-start";
  ctx.warn("subset:space-justify-collapsed-on-overflow",
           "html: <" + parent->name + "> children overflow main axis; justify-content '" + jc +
               "' collapsed to flex-start to avoid PAGX flex overlap",
           parent);
}

void WalkCollapseSpaceJustify(const std::shared_ptr<DOMNode>& node, HTMLTransformContext& ctx,
                              int depth) {
  if (ShouldSkipWalkerNode(node, depth, ctx, "space-justify collapse")) return;
  // SVG subtrees use an independent layout model; skip them entirely.
  if (IsOpaqueSubtreeRoot(node)) return;
  TryCollapseSpaceJustify(node, ctx);
  auto child = node->firstChild;
  while (child) {
    WalkCollapseSpaceJustify(child, ctx, depth + 1);
    child = child->nextSibling;
  }
}

}  // namespace

void SpaceJustifyOverflowCollapsePass::apply(const std::shared_ptr<DOMNode>& root,
                                             HTMLTransformContext& ctx) {
  if (!root || ctx.hasFatal()) return;
  auto body = root->getFirstChild("body");
  if (!body) return;
  WalkCollapseSpaceJustify(body, ctx, 0);
}

//==================================================================================================
// StructureNormalizationPass
//==================================================================================================

namespace {

// Wraps non-blank text children of a container into a synthetic `<p>`. Element children are
// left untouched. Only ever invoked for container parents (see NormalizeChildren), so the
// wrapper tag is always `<p>`.
void WrapStrayText(const std::shared_ptr<DOMNode>& parent, HTMLTransformContext& ctx) {
  std::shared_ptr<DOMNode> prev = nullptr;
  auto child = parent->firstChild;
  while (child) {
    auto next = child->nextSibling;
    if (child->type == DOMNodeType::Text && !IsBlankText(child->name)) {
      auto wrapper = MakeElement("p");
      wrapper->firstChild = child;
      child->nextSibling = nullptr;
      wrapper->nextSibling = next;
      if (prev) {
        prev->nextSibling = wrapper;
      } else {
        parent->firstChild = wrapper;
      }
      ctx.warn("subset:text-wrapped",
               "html: stray text inside <" + parent->name + "> wrapped in <p>", parent);
      prev = wrapper;
    } else if (child->type == DOMNodeType::Text && IsBlankText(child->name)) {
      // Drop blank whitespace nodes between elements (they confuse layout).
      UnlinkChild(parent, prev, child);
    } else {
      prev = child;
    }
    child = next;
  }
}

// Recurse into element children, dropping unknown tags, then run WrapStrayText on the result.
void NormalizeChildren(const std::shared_ptr<DOMNode>& parent, HTMLTransformContext& ctx,
                       int depth) {
  if (ShouldSkipWalkerNode(parent, depth, ctx, "structure normalization")) return;
  // SVG subtrees are opaque: their tag set (`<circle>`, `<path>`, ...) is not part of the HTML
  // subset, but the importer captures the verbatim XML and feeds it to the SVG resolver. We
  // therefore leave the subtree untouched.
  if (IsOpaqueSubtreeRoot(parent)) {
    return;
  }
  std::shared_ptr<DOMNode> prev = nullptr;
  auto child = parent->firstChild;
  while (child) {
    auto next = child->nextSibling;
    if (child->type == DOMNodeType::Element && !IsAllowedTag(child->name)) {
      if (ctx.options().preserveUnknownElements) {
        // Convert to <div data-html-unknown="<tag>"> with original attributes preserved (best
        // effort — children retained but they may themselves be unsupported).
        DOMAttribute marker = {};
        marker.name = "data-html-unknown";
        marker.value = child->name;
        child->attributes.push_back(marker);
        child->name = "div";
        ctx.warn("subset:unsupported-tag-preserved",
                 "html: <" + marker.value + "> kept as <div data-html-unknown=\"" + marker.value +
                     "\"> for debugging",
                 child);
      } else {
        ctx.warn("subset:unsupported-tag",
                 "html: <" + child->name + "> is not in the subset; dropped", child);
        UnlinkChild(parent, prev, child);
        child = next;
        continue;
      }
    }
    if (child->type == DOMNodeType::Element) {
      NormalizeChildren(child, ctx, depth + 1);
    }
    prev = child;
    child = next;
  }
  // After cleaning element children, wrap stray text in containers. Text-leaf parents
  // (<p>/<hN>/<span>/<a>) keep their text children unmodified — that's where text content
  // legitimately lives. Inline-run / void / metadata parents are also skipped.
  if (parent->type == DOMNodeType::Element && IsContainerTag(parent->name)) {
    WrapStrayText(parent, ctx);
  }
}

}  // namespace

void StructureNormalizationPass::apply(const std::shared_ptr<DOMNode>& root,
                                       HTMLTransformContext& ctx) {
  if (!root || ctx.hasFatal()) return;
  auto body = root->getFirstChild("body");
  if (!body) return;
  NormalizeChildren(body, ctx, 0);
}

//==================================================================================================
// InlineStyleEmitterPass
//==================================================================================================

namespace {

bool IsEmptyStyleAttribute(const DOMAttribute& a) {
  return a.name == "style" && a.value.empty();
}

bool IsClassAttribute(const DOMAttribute& a) {
  return a.name == "class";
}

void EmitStyleAttribute(const std::shared_ptr<DOMNode>& element, HTMLTransformContext& ctx) {
  auto* resolved = ctx.findResolved(element.get());
  if (!resolved) return;
  // Sort keys deterministically.
  std::vector<std::string> keys;
  keys.reserve(resolved->size());
  for (const auto& kv : *resolved) {
    if (kv.second.empty()) continue;
    keys.push_back(kv.first);
  }
  std::sort(keys.begin(), keys.end());
  std::string out;
  out.reserve(64 + keys.size() * 16);
  for (const auto& key : keys) {
    if (!out.empty()) out.push_back(' ');
    out += key;
    out += ": ";
    out += resolved->at(key);
    out.push_back(';');
  }
  // Replace or add the style attribute.
  bool replaced = false;
  for (auto& attr : element->attributes) {
    if (attr.name == "style") {
      if (out.empty()) {
        attr.value.clear();
      } else {
        attr.value = out;
      }
      replaced = true;
      break;
    }
  }
  if (!replaced && !out.empty()) {
    DOMAttribute attr = {};
    attr.name = "style";
    attr.value = out;
    element->attributes.push_back(attr);
  }
  if (replaced && out.empty()) {
    // Drop the now-empty style attribute.
    element->attributes.erase(std::remove_if(element->attributes.begin(), element->attributes.end(),
                                             IsEmptyStyleAttribute),
                              element->attributes.end());
  }
}

void EmitTree(const std::shared_ptr<DOMNode>& element, HTMLTransformContext& ctx, int depth) {
  if (ShouldSkipWalkerNode(element, depth, ctx, "inline style emission")) return;
  if (!IsNonRenderedTag(element->name)) {
    EmitStyleAttribute(element, ctx);
    if (!ctx.options().preserveClassAttribute) {
      element->attributes.erase(
          std::remove_if(element->attributes.begin(), element->attributes.end(), IsClassAttribute),
          element->attributes.end());
    }
  }
  // SVG subtrees are opaque; their attributes are forwarded verbatim by the importer.
  if (IsOpaqueSubtreeRoot(element)) return;
  auto child = element->firstChild;
  while (child) {
    EmitTree(child, ctx, depth + 1);
    child = child->nextSibling;
  }
}

}  // namespace

void InlineStyleEmitterPass::apply(const std::shared_ptr<DOMNode>& root,
                                   HTMLTransformContext& ctx) {
  if (!root || ctx.hasFatal()) return;
  auto body = root->getFirstChild("body");
  if (!body) return;
  EmitTree(body, ctx, 0);
}

}  // namespace pagx::html
