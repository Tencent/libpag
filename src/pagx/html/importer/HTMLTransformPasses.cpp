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

namespace pagx::html {

namespace {

// CSS properties that inherit by default in the subset. Mirrors `HTMLInheritedStyle` in the
// importer so that the cascade carries identical semantics through both layers.
const std::vector<std::string>& InheritableProperties() {
  static const std::vector<std::string> v = {
      "color",           "font-family",           "font-size",   "font-weight",
      "font-style",      "letter-spacing",        "line-height", "text-align",
      "text-decoration", "text-decoration-color", "white-space",
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
      if (!prev) {
        parent->firstChild = next;
      } else {
        prev->nextSibling = next;
      }
      child = next;
      continue;
    }
    prev = child;
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
  auto dupNext = dup->nextSibling;
  if (prev) {
    prev->nextSibling = dupNext;
  } else {
    parent->firstChild = dupNext;
  }
  dup->nextSibling = nullptr;
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
      if (prev) {
        prev->nextSibling = next;
      } else {
        root->firstChild = next;
      }
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
        if (!prevHeadChild) {
          head->firstChild = next;
        } else {
          prevHeadChild->nextSibling = next;
        }
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
        if (!prev) {
          head->firstChild = next;
        } else {
          prev->nextSibling = next;
        }
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

void ResolveTree(const std::shared_ptr<DOMNode>& element, const PropertyMap& inheritedIn,
                 HTMLTransformContext& ctx, int depth) {
  if (!element || element->type != DOMNodeType::Element) return;
  if (depth >= MAX_HTML_RECURSION_DEPTH) {
    ctx.warn("subset:max-depth",
             "html: maximum recursion depth reached during style resolution; subtree skipped",
             element);
    return;
  }
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
  if (element->name == "svg") return;
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
  const auto& defaults = ElementDefaults();
  auto defaultIt = defaults.find(element->name);
  if (defaultIt != defaults.end()) {
    ParseStyleString(defaultIt->second, resolved);
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
  if (!element || element->type != DOMNodeType::Element) return;
  if (depth >= MAX_HTML_RECURSION_DEPTH) {
    ctx.warn("subset:max-depth",
             "html: maximum recursion depth reached during property filtering; subtree skipped",
             element);
    return;
  }
  float fontSizeForChildren = parentFontSizePx;
  if (!IsNonRenderedTag(element->name)) {
    fontSizeForChildren = FilterElement(element, parentFontSizePx, ctx);
  }
  // SVG subtrees are opaque to property filtering.
  if (element->name == "svg") return;
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
// AbsoluteToFlexInferencePass
//==================================================================================================

namespace {

// Parses an already-normalised length value (`Npx`) into a finite float. Returns false when
// the value is empty, percent-based, or otherwise not a plain pixel length. PropertyFilter has
// already converted every supported unit to `px`, so anything else means "skip".
bool ParseNormalisedPx(const std::string& valueRaw, float& outPx) {
  std::string value = Trim(valueRaw);
  if (value.empty()) return false;
  const char* c = value.c_str();
  char* end = nullptr;
  float v = std::strtof(c, &end);
  if (end == c) return false;
  if (!std::isfinite(v)) return false;
  std::string suffix = Trim(end);
  if (suffix.empty() || suffix == "px") {
    outPx = v;
    return true;
  }
  return false;
}

// Reads a property from a resolved style map, returning empty when missing.
const std::string& LookupResolved(const PropertyMap& props, const std::string& key) {
  return LookupProperty(props, key);
}

// Lower-cased view of a resolved property.
std::string LookupResolvedLower(const PropertyMap& props, const std::string& key) {
  return LookupLowerTrimmed(props, key);
}

// Expands a `padding` shorthand value (e.g. `4px 8px`) into top/right/bottom/left. Returns
// false when any token is not a plain px length. The output buffers are written only on
// success.
bool ApplyPaddingShorthand(const std::string& value, float& top, float& right, float& bottom,
                           float& left) {
  auto tokens = SplitTopLevelWhitespace(value);
  std::vector<float> nums;
  nums.reserve(tokens.size());
  for (auto& t : tokens) {
    float px = 0.0f;
    if (!ParseNormalisedPx(t, px)) return false;
    nums.push_back(px);
  }
  if (nums.empty()) return false;
  auto p = BuildPaddingShorthand(nums);
  top = p.top;
  right = p.right;
  bottom = p.bottom;
  left = p.left;
  return true;
}

// Parses the resolved `padding` shorthand (1-4 px tokens) into top/right/bottom/left. Returns
// false if the value is missing or any token is not a plain px length. Per-side `padding-*`
// overrides (when present) win over the shorthand.
bool ParsePaddingFromResolved(const PropertyMap& props, float& top, float& right, float& bottom,
                              float& left) {
  top = right = bottom = left = 0.0f;
  bool any = false;
  auto sh = LookupResolved(props, "padding");
  if (!sh.empty()) {
    if (!ApplyPaddingShorthand(sh, top, right, bottom, left)) return false;
    any = true;
  }
  // Per-side overrides.
  float px = 0.0f;
  if (auto v = LookupResolved(props, "padding-top"); !v.empty() && ParseNormalisedPx(v, px)) {
    top = px;
    any = true;
  }
  if (auto v = LookupResolved(props, "padding-right"); !v.empty() && ParseNormalisedPx(v, px)) {
    right = px;
    any = true;
  }
  if (auto v = LookupResolved(props, "padding-bottom"); !v.empty() && ParseNormalisedPx(v, px)) {
    bottom = px;
    any = true;
  }
  if (auto v = LookupResolved(props, "padding-left"); !v.empty() && ParseNormalisedPx(v, px)) {
    left = px;
    any = true;
  }
  return any || sh.empty();  // empty padding is fine (zeros)
}

// Padding shorthand emission and per-axis px formatting are shared with the property table /
// importer via HTMLDetail (`EmitPaddingShorthand`, `EmitPx`).

struct ChildBox {
  std::shared_ptr<DOMNode> node;
  float left = 0.0f;
  float top = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
};

// Returns true when `props` describes an `position: absolute` element with all four of
// `left`/`top`/`width`/`height` resolved into plain px lengths and no `right`/`bottom` /
// `flex` overrides that would conflict with a flex rewrite.
bool ExtractAbsoluteBox(const PropertyMap& props, ChildBox& out) {
  if (LookupResolvedLower(props, "position") != "absolute") return false;
  // Conflict guards: we don't try to rewrite children that pin against the parent's right /
  // bottom edges, or that already declare a flex grow factor.
  auto rightVal = LookupResolved(props, "right");
  auto bottomVal = LookupResolved(props, "bottom");
  if (!rightVal.empty()) return false;
  if (!bottomVal.empty()) return false;
  if (!LookupResolved(props, "flex").empty()) return false;
  float left = 0.0f;
  float top = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
  if (!ParseNormalisedPx(LookupResolved(props, "left"), left)) return false;
  if (!ParseNormalisedPx(LookupResolved(props, "top"), top)) return false;
  if (!ParseNormalisedPx(LookupResolved(props, "width"), width)) return false;
  if (!ParseNormalisedPx(LookupResolved(props, "height"), height)) return false;
  if (!std::isfinite(left) || !std::isfinite(top)) return false;
  if (!(width > 0.0f) || !(height > 0.0f)) return false;
  out.left = left;
  out.top = top;
  out.width = width;
  out.height = height;
  return true;
}

enum class FlexAxis { Row, Column };

// Comparators for sorting `ChildBox` along the row / column main axes. Hoisted out of
// `InferAxis` to honour the project's "no lambdas" rule.
bool ChildBoxLeftLess(const ChildBox& a, const ChildBox& b) {
  return a.left < b.left;
}
bool ChildBoxTopLess(const ChildBox& a, const ChildBox& b) {
  return a.top < b.top;
}

// Sorts `children` along `row` (true) or column (false) into `outSorted` and returns true
// when sorting yields a sequence with no main-axis overlap (within tolerance `tol`).
bool TryAxisSortedNoOverlap(const std::vector<ChildBox>& children, bool row, float tol,
                            std::vector<ChildBox>& outSorted) {
  outSorted = children;
  std::sort(outSorted.begin(), outSorted.end(), row ? ChildBoxLeftLess : ChildBoxTopLess);
  for (size_t i = 0; i + 1 < outSorted.size(); i++) {
    float curEnd =
        row ? outSorted[i].left + outSorted[i].width : outSorted[i].top + outSorted[i].height;
    float nextStart = row ? outSorted[i + 1].left : outSorted[i + 1].top;
    if (nextStart < curEnd - tol) return false;
  }
  return true;
}

// Returns the spread (max - min) of the main-axis starting offsets of `children`.
float AxisStartSpread(const std::vector<ChildBox>& children, bool row) {
  float lo = std::numeric_limits<float>::infinity();
  float hi = -std::numeric_limits<float>::infinity();
  for (const auto& c : children) {
    float v = row ? c.left : c.top;
    lo = std::min(lo, v);
    hi = std::max(hi, v);
  }
  return hi - lo;
}

// Returns the inferred main axis when every child's bounding box is ordered along it without
// overlap. When both axes are valid (single child / collinear), prefers the axis with the
// larger spread. On success populates `outSorted` with the children sorted along the chosen
// main axis so the caller does not have to sort again.
bool InferAxis(const std::vector<ChildBox>& children, float tol, FlexAxis& out,
               std::vector<ChildBox>& outSorted) {
  if (children.size() < 2) return false;
  std::vector<ChildBox> sortedRow;
  std::vector<ChildBox> sortedCol;
  bool rowOk = TryAxisSortedNoOverlap(children, /*row=*/true, tol, sortedRow);
  bool colOk = TryAxisSortedNoOverlap(children, /*row=*/false, tol, sortedCol);
  if (!rowOk && !colOk) return false;
  if (rowOk && colOk) {
    // Both axes are non-overlapping — pick the one with the largest spread of starts so that a
    // genuine row-of-3 doesn't get classified as a column with three identical y values.
    out = AxisStartSpread(children, /*row=*/true) >= AxisStartSpread(children, /*row=*/false)
              ? FlexAxis::Row
              : FlexAxis::Column;
  } else {
    out = rowOk ? FlexAxis::Row : FlexAxis::Column;
  }
  outSorted = (out == FlexAxis::Row) ? std::move(sortedRow) : std::move(sortedCol);
  return true;
}

enum class CrossAlign { Start, Center, End, Stretch, Mixed };

// Determines a single `align-items` value compatible with all children's cross-axis position
// inside the parent's content box. Returns CrossAlign::Mixed when no single keyword fits.
CrossAlign InferCrossAlign(const std::vector<ChildBox>& children, FlexAxis axis, float contentLow,
                           float contentHigh, float tol) {
  if (children.empty()) return CrossAlign::Mixed;
  bool stretchOk = true;
  bool startOk = true;
  bool endOk = true;
  bool centerOk = true;
  float contentMid = 0.5f * (contentLow + contentHigh);
  float contentSize = contentHigh - contentLow;
  for (const auto& c : children) {
    float lo = axis == FlexAxis::Row ? c.top : c.left;
    float size = axis == FlexAxis::Row ? c.height : c.width;
    float hi = lo + size;
    float mid = lo + size * 0.5f;
    if (std::fabs(lo - contentLow) > tol) startOk = false;
    if (std::fabs(hi - contentHigh) > tol) endOk = false;
    if (std::fabs(mid - contentMid) > tol) centerOk = false;
    if (!(std::fabs(lo - contentLow) <= tol && std::fabs(size - contentSize) <= tol)) {
      stretchOk = false;
    }
  }
  if (stretchOk) return CrossAlign::Stretch;
  if (startOk) return CrossAlign::Start;
  if (centerOk) return CrossAlign::Center;
  if (endOk) return CrossAlign::End;
  return CrossAlign::Mixed;
}

// Result of main-axis spacing inference. Either we can produce a clean
// (paddingLeading, paddingTrailing, gap) triple (`ok = true`), or we can't and the parent is
// left as-is.
struct MainAxisFit {
  bool ok = false;
  float paddingLeading = 0.0f;
  float paddingTrailing = 0.0f;
  float gap = 0.0f;
};

// Returns the main-axis low (start) or high (end) coordinate of a child box on the given axis.
// Hoisted out of `InferMainAxisSpacing` to honour the project's "no lambdas" rule.
float GetMainAxisCoord(const ChildBox& c, FlexAxis axis, bool low) {
  if (axis == FlexAxis::Row) {
    return low ? c.left : c.left + c.width;
  }
  return low ? c.top : c.top + c.height;
}

MainAxisFit InferMainAxisSpacing(const std::vector<ChildBox>& sorted, FlexAxis axis,
                                 float contentLow, float contentHigh, float tol) {
  MainAxisFit fit = {};
  fit.paddingLeading = GetMainAxisCoord(sorted.front(), axis, /*low=*/true) - contentLow;
  fit.paddingTrailing = contentHigh - GetMainAxisCoord(sorted.back(), axis, /*low=*/false);
  if (fit.paddingLeading < -tol) return fit;   // first child overflows the content box
  if (fit.paddingTrailing < -tol) return fit;  // last child overflows the content box
  if (fit.paddingLeading < 0) fit.paddingLeading = 0;
  if (fit.paddingTrailing < 0) fit.paddingTrailing = 0;
  if (sorted.size() == 1) {
    fit.ok = true;
    return fit;
  }
  // Compute every middle gap; require they all match within tolerance.
  float first = GetMainAxisCoord(sorted[1], axis, /*low=*/true) -
                GetMainAxisCoord(sorted[0], axis, /*low=*/false);
  if (first < -tol) return fit;  // overlap (shouldn't happen — InferAxis guarded this)
  float gap = std::max(0.0f, first);
  for (size_t i = 1; i + 1 < sorted.size(); i++) {
    float g = GetMainAxisCoord(sorted[i + 1], axis, /*low=*/true) -
              GetMainAxisCoord(sorted[i], axis, /*low=*/false);
    if (std::fabs(g - first) > tol) return fit;
    gap = std::max(gap, std::max(0.0f, g));
  }
  fit.gap = gap;
  fit.ok = true;
  return fit;
}

// Looks up the parent's content box (inside-padding rectangle) along a single axis. When the
// parent has no explicit dimension, falls back to the children's bounding extents so that the
// inference still works on layout-only wrapper containers. Returns true on success.
bool ResolveContentRange(const PropertyMap* parentProps, const std::vector<ChildBox>& children,
                         float padLow, float padHigh, FlexAxis axis, float& contentLow,
                         float& contentHigh) {
  contentLow = padLow;
  contentHigh = std::numeric_limits<float>::quiet_NaN();
  float dim = std::numeric_limits<float>::quiet_NaN();
  if (parentProps) {
    const std::string& key = axis == FlexAxis::Row ? "width" : "height";
    auto val = LookupResolved(*parentProps, key);
    float px = 0.0f;
    if (!val.empty() && ParseNormalisedPx(val, px) && px > 0.0f) {
      dim = px;
    }
  }
  if (std::isfinite(dim)) {
    contentHigh = dim - padHigh;
    return contentHigh > contentLow;
  }
  // Fallback: bounding extent of children (allows inference on parents without explicit size).
  float lo = std::numeric_limits<float>::infinity();
  float hi = -std::numeric_limits<float>::infinity();
  for (const auto& c : children) {
    float a = axis == FlexAxis::Row ? c.left : c.top;
    float b = axis == FlexAxis::Row ? c.left + c.width : c.top + c.height;
    lo = std::min(lo, a);
    hi = std::max(hi, b);
  }
  if (!std::isfinite(lo) || !std::isfinite(hi)) return false;
  contentLow = lo;
  contentHigh = hi;
  return true;
}

const char* AxisName(FlexAxis a) {
  return a == FlexAxis::Row ? "row" : "column";
}

// Mutates the parent's resolved style to declare the inferred flex layout. Strips any
// per-side padding overrides we just folded into the shorthand.
void ApplyFlexToParent(PropertyMap& parentProps, FlexAxis axis, float padTop, float padRight,
                       float padBottom, float padLeft, float gap, CrossAlign align) {
  parentProps["display"] = "flex";
  parentProps["flex-direction"] = AxisName(axis);
  if (gap > 0.0f) {
    parentProps["gap"] = EmitPx(gap);
  } else {
    parentProps.erase("gap");
  }
  if (padTop > 0.0f || padRight > 0.0f || padBottom > 0.0f || padLeft > 0.0f) {
    parentProps["padding"] = EmitPaddingShorthand(padTop, padRight, padBottom, padLeft);
  } else {
    parentProps.erase("padding");
  }
  parentProps.erase("padding-top");
  parentProps.erase("padding-right");
  parentProps.erase("padding-bottom");
  parentProps.erase("padding-left");
  switch (align) {
    case CrossAlign::Stretch:
      parentProps["align-items"] = "stretch";
      break;
    case CrossAlign::Center:
      parentProps["align-items"] = "center";
      break;
    case CrossAlign::Start:
      parentProps["align-items"] = "flex-start";
      break;
    case CrossAlign::End:
      parentProps["align-items"] = "flex-end";
      break;
    case CrossAlign::Mixed:
      break;  // unreachable: caller already filtered Mixed
  }
}

// Strips position-related declarations from a child that the parent's flex layout now
// places. When the chosen alignment is Stretch the cross-axis size is also dropped so that
// the flex engine fills the cross axis (matching CSS `align-items: stretch`).
void StripChildAbsolute(PropertyMap& childProps, FlexAxis axis, CrossAlign align) {
  childProps.erase("position");
  childProps.erase("left");
  childProps.erase("right");
  childProps.erase("top");
  childProps.erase("bottom");
  if (align == CrossAlign::Stretch) {
    childProps.erase(axis == FlexAxis::Row ? "height" : "width");
  }
}

// Reorders `parent`'s element children to match `sortedElements`, while keeping any
// non-element (text) children in their original ordinal slots. Required because flex
// honours document order but the absolute children we just folded did not.
// Returns true when the order changed.
bool ReorderElementChildrenToMainAxis(const std::shared_ptr<DOMNode>& parent,
                                      const std::vector<ChildBox>& sortedElements) {
  if (!parent) return false;
  std::vector<std::shared_ptr<DOMNode>> all;
  size_t elementCount = 0;
  bool needsReorder = false;
  for (auto c = parent->firstChild; c; c = c->nextSibling) {
    all.push_back(c);
    if (c->type == DOMNodeType::Element) {
      if (elementCount >= sortedElements.size()) return false;
      if (c != sortedElements[elementCount].node) needsReorder = true;
      elementCount++;
    }
  }
  if (elementCount != sortedElements.size()) return false;
  if (!needsReorder) return false;

  size_t idx = 0;
  std::shared_ptr<DOMNode> prev;
  parent->firstChild = nullptr;
  for (auto& c : all) {
    if (c->type == DOMNodeType::Element) c = sortedElements[idx++].node;
    c->nextSibling = nullptr;
    if (!prev) {
      parent->firstChild = c;
    } else {
      prev->nextSibling = c;
    }
    prev = c;
  }
  return true;
}

void TryInferFlexOnContainer(const std::shared_ptr<DOMNode>& parent, HTMLTransformContext& ctx) {
  if (!parent || parent->type != DOMNodeType::Element) return;
  const std::string& tag = parent->name;
  // Only rewrite "container" tags — descending into <p>/<span>/<a> would change text layout.
  // <body> is handled the same way (it's a container for layout purposes).
  if (!IsContainerTag(tag)) return;

  // If the parent already declares display: flex, leave it alone — we don't second-guess
  // explicit author intent.
  auto* parentResolved = ctx.findResolved(parent.get());
  if (parentResolved) {
    if (LookupResolvedLower(*parentResolved, "display") == "flex") return;
  }

  // Gather direct element children with absolute geometry. We require *every* element child
  // to participate; mixing absolute and flow children can't be expressed as a single flex
  // container, so we skip rather than do something surprising.
  std::vector<ChildBox> boxes;
  bool sawNonAbsoluteChild = false;
  size_t totalElementChildren = 0;
  for (auto c = parent->firstChild; c; c = c->nextSibling) {
    if (c->type != DOMNodeType::Element) continue;
    totalElementChildren++;
    auto* cp = ctx.findResolved(c.get());
    if (!cp) {
      sawNonAbsoluteChild = true;
      continue;
    }
    ChildBox box = {};
    box.node = c;
    if (!ExtractAbsoluteBox(*cp, box)) {
      sawNonAbsoluteChild = true;
      continue;
    }
    boxes.push_back(box);
  }
  if (sawNonAbsoluteChild) return;
  if (boxes.size() < 2) return;
  if (totalElementChildren != boxes.size()) return;

  float tol = std::max(0.0f, ctx.options().flexInferenceTolerancePx);

  FlexAxis axis = FlexAxis::Row;
  std::vector<ChildBox> sorted;
  if (!InferAxis(boxes, tol, axis, sorted)) {
    ctx.warn("subset:flex-inference-skipped",
             "html: <" + tag +
                 "> children overlap on both axes; cannot recover flex layout, kept absolute",
             parent);
    return;
  }

  // Resolve the parent's content box along both axes. When the parent has no explicit size,
  // fall back to the children's bounding extents (so wrapper containers without a width still
  // get inferred); this keeps the cross-axis check honest because alignment is computed
  // against the same range the children already span.
  float padTopExisting = 0.0f;
  float padRightExisting = 0.0f;
  float padBottomExisting = 0.0f;
  float padLeftExisting = 0.0f;
  if (parentResolved) {
    if (!ParsePaddingFromResolved(*parentResolved, padTopExisting, padRightExisting,
                                  padBottomExisting, padLeftExisting)) {
      // Padding declared but not in plain px (e.g. percent) — bail out rather than guess.
      return;
    }
  }
  float mainContentLow = 0.0f;
  float mainContentHigh = 0.0f;
  float crossContentLow = 0.0f;
  float crossContentHigh = 0.0f;
  float mainPadLow = axis == FlexAxis::Row ? padLeftExisting : padTopExisting;
  float mainPadHigh = axis == FlexAxis::Row ? padRightExisting : padBottomExisting;
  float crossPadLow = axis == FlexAxis::Row ? padTopExisting : padLeftExisting;
  float crossPadHigh = axis == FlexAxis::Row ? padBottomExisting : padRightExisting;
  if (!ResolveContentRange(parentResolved, boxes, mainPadLow, mainPadHigh, axis, mainContentLow,
                           mainContentHigh)) {
    return;
  }
  FlexAxis crossAxis = axis == FlexAxis::Row ? FlexAxis::Column : FlexAxis::Row;
  if (!ResolveContentRange(parentResolved, boxes, crossPadLow, crossPadHigh, crossAxis,
                           crossContentLow, crossContentHigh)) {
    return;
  }

  CrossAlign align = InferCrossAlign(boxes, axis, crossContentLow, crossContentHigh, tol);
  if (align == CrossAlign::Mixed) {
    ctx.warn("subset:flex-inference-skipped",
             "html: <" + tag + "> children have mixed cross-axis alignment; kept absolute", parent);
    return;
  }

  // `InferAxis` already sorted the children along the chosen main axis above; reuse the
  // pre-sorted vector for spacing inference instead of re-sorting.
  MainAxisFit fit = InferMainAxisSpacing(sorted, axis, mainContentLow, mainContentHigh, tol);
  if (!fit.ok) {
    ctx.warn("subset:flex-inference-skipped",
             "html: <" + tag + "> children have inconsistent main-axis spacing; kept absolute",
             parent);
    return;
  }

  // Combine inherited cross-axis padding (kept verbatim) with the inferred main-axis padding.
  float padTop = axis == FlexAxis::Row ? padTopExisting : fit.paddingLeading;
  float padBottom = axis == FlexAxis::Row ? padBottomExisting : fit.paddingTrailing;
  float padLeft = axis == FlexAxis::Row ? fit.paddingLeading : padLeftExisting;
  float padRight = axis == FlexAxis::Row ? fit.paddingTrailing : padRightExisting;

  // Commit the rewrite to the parent's resolved property map. PropertyFilter has already
  // populated this; the InlineStyleEmitter will serialise it back to `style="…"`.
  if (!parentResolved) {
    PropertyMap fresh;
    ctx.setResolved(parent.get(), std::move(fresh));
    parentResolved = ctx.findResolved(parent.get());
  }
  ApplyFlexToParent(*parentResolved, axis, padTop, padRight, padBottom, padLeft, fit.gap, align);

  // Strip absolute placement from each child.
  for (auto& b : boxes) {
    auto* cp = ctx.findResolved(b.node.get());
    if (!cp) continue;
    StripChildAbsolute(*cp, axis, align);
  }

  // Flex follows DOM order; the absolute children we just folded did not. Re-link the
  // child list so the inferred axis order matches what the user saw.
  bool reordered = ReorderElementChildrenToMainAxis(parent, sorted);

  ctx.warn("subset:flex-inferred",
           "html: <" + tag + "> rewritten as display:flex (" + AxisName(axis) + ", " +
               std::to_string(boxes.size()) + " children" +
               (reordered ? ", reordered to match position" : "") + ")",
           parent);
}

void WalkInferFlex(const std::shared_ptr<DOMNode>& node, HTMLTransformContext& ctx, int depth) {
  if (!node || node->type != DOMNodeType::Element) return;
  if (depth >= MAX_HTML_RECURSION_DEPTH) {
    ctx.warn("subset:max-depth",
             "html: maximum recursion depth reached during flex inference; subtree skipped", node);
    return;
  }
  // SVG subtrees are opaque (their absolute geometry is meaningful at a different layer).
  if (node->name == "svg") return;
  // Recurse children first: a parent that folds into `align-items: stretch` erases its
  // children's cross-axis size, so each child must finish its own inference while its
  // explicit width/height is still intact.
  auto child = node->firstChild;
  while (child) {
    WalkInferFlex(child, ctx, depth + 1);
    child = child->nextSibling;
  }
  TryInferFlexOnContainer(node, ctx);
}

}  // namespace

void AbsoluteToFlexInferencePass::apply(const std::shared_ptr<DOMNode>& root,
                                        HTMLTransformContext& ctx) {
  if (!root || ctx.hasFatal()) return;
  if (!ctx.options().inferFlexFromAbsolute) return;
  auto body = root->getFirstChild("body");
  if (!body) return;
  WalkInferFlex(body, ctx, 0);
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
      if (!prev) {
        parent->firstChild = next;
      } else {
        prev->nextSibling = next;
      }
      child->nextSibling = nullptr;
    } else {
      prev = child;
    }
    child = next;
  }
}

// Recurse into element children, dropping unknown tags, then run WrapStrayText on the result.
void NormalizeChildren(const std::shared_ptr<DOMNode>& parent, HTMLTransformContext& ctx,
                       int depth) {
  if (depth >= MAX_HTML_RECURSION_DEPTH) {
    ctx.warn(
        "subset:max-depth",
        "html: maximum recursion depth reached during structure normalization; subtree skipped",
        parent);
    return;
  }
  // SVG subtrees are opaque: their tag set (`<circle>`, `<path>`, ...) is not part of the HTML
  // subset, but the importer captures the verbatim XML and feeds it to the SVG resolver. We
  // therefore leave the subtree untouched.
  if (parent->type == DOMNodeType::Element && parent->name == "svg") {
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
        if (!prev) {
          parent->firstChild = next;
        } else {
          prev->nextSibling = next;
        }
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
  if (!element || element->type != DOMNodeType::Element) return;
  if (depth >= MAX_HTML_RECURSION_DEPTH) {
    ctx.warn("subset:max-depth",
             "html: maximum recursion depth reached during inline style emission; subtree skipped",
             element);
    return;
  }
  if (!IsNonRenderedTag(element->name)) {
    EmitStyleAttribute(element, ctx);
    if (!ctx.options().preserveClassAttribute) {
      element->attributes.erase(
          std::remove_if(element->attributes.begin(), element->attributes.end(), IsClassAttribute),
          element->attributes.end());
    }
  }
  // SVG subtrees are opaque; their attributes are forwarded verbatim by the importer.
  if (element->name == "svg") return;
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
