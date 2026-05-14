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

#include "pagx/html/HTMLTransformPasses.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <utility>
#include "pagx/html/HTMLCssCascade.h"
#include "pagx/html/HTMLParserContext.h"
#include "pagx/html/HTMLSubsetPropertyTable.h"

namespace pagx::html_passes {

namespace {

using pagx::detail::ElementDefaults;
using pagx::detail::IsContainerTag;
using pagx::detail::IsInlineRunTag;
using pagx::detail::IsTextLeafTag;
using pagx::detail::LowercaseTagsInPlace;
using pagx::detail::ParseStyleString;
using pagx::detail::ToLower;
using pagx::detail::Trim;
using pagx::subset_props::IsDataAttribute;

// CSS properties that inherit by default in the subset. Mirrors `HTMLInheritedStyle` in the
// importer so that the cascade carries identical semantics through both layers.
const std::vector<std::string>& InheritableProperties() {
  static const std::vector<std::string> v = {
      "color",          "font-family",     "font-size",            "font-weight",
      "font-style",     "letter-spacing",  "line-height",          "text-align",
      "text-decoration", "text-decoration-color", "white-space",
  };
  return v;
}

bool IsVoidElement(const std::string& tag) {
  return tag == "br" || tag == "hr" || tag == "img" || tag == "meta" || tag == "input" ||
         tag == "link";
}

bool IsAllowedTag(const std::string& tag) {
  return IsContainerTag(tag) || IsTextLeafTag(tag) || tag == "br" || tag == "img" ||
         tag == "svg" || tag == "html" || tag == "head" || tag == "body" || tag == "title" ||
         tag == "meta" || tag == "style";
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

// Walk children of `parent` and call `fn(parent, prev, child)` for each child element. The
// callback is allowed to remove or replace `child` via the returned new pointer (nullptr means
// "delete this child"; any other pointer replaces it).
template <typename Fn>
void TransformChildren(const std::shared_ptr<DOMNode>& parent, Fn fn) {
  std::shared_ptr<DOMNode> prev = nullptr;
  auto child = parent->firstChild;
  while (child) {
    auto next = child->nextSibling;
    auto replacement = fn(parent, prev, child);
    if (!replacement) {
      if (!prev) {
        parent->firstChild = next;
      } else {
        prev->nextSibling = next;
      }
    } else {
      if (replacement.get() != child.get()) {
        replacement->nextSibling = next;
        if (!prev) {
          parent->firstChild = replacement;
        } else {
          prev->nextSibling = replacement;
        }
      }
      prev = replacement;
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
        // Append head's children into the first head.
        auto sub = child->firstChild;
        std::shared_ptr<DOMNode> lastInHead = head->firstChild;
        while (lastInHead && lastInHead->nextSibling) lastInHead = lastInHead->nextSibling;
        if (!head->firstChild) {
          head->firstChild = sub;
        } else if (lastInHead) {
          lastInHead->nextSibling = sub;
        }
        // Remove duplicate from root.
        if (prev) prev->nextSibling = next;
      }
    } else if (child->name == "body") {
      if (!body) {
        body = child;
        prev = child;
      } else {
        // Append body's children into the first body.
        auto sub = child->firstChild;
        std::shared_ptr<DOMNode> lastInBody = body->firstChild;
        while (lastInBody && lastInBody->nextSibling) lastInBody = lastInBody->nextSibling;
        if (!body->firstChild) {
          body->firstChild = sub;
        } else if (lastInBody) {
          lastInBody->nextSibling = sub;
        }
        if (prev) prev->nextSibling = next;
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
    TransformChildren(head, [&](const std::shared_ptr<DOMNode>&,
                                const std::shared_ptr<DOMNode>&,
                                const std::shared_ptr<DOMNode>& c) -> std::shared_ptr<DOMNode> {
      if (c->name == "title" || c->name == "meta" || c->name == "style") {
        return c;
      }
      if (c->name == "link") {
        auto* rel = c->findAttribute("rel");
        if (rel && ToLower(Trim(*rel)) == "stylesheet") {
          ctx.warn("subset:external-stylesheet",
                   "html: external <link rel=\"stylesheet\"> dropped", c);
        }
        return nullptr;  // drop silently
      }
      ctx.warn("subset:unsupported-tag",
               "html: <" + c->name + "> inside <head> is not allowed; dropped", c);
      return nullptr;
    });
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
    auto rules = css_cascade::TokenizeStyleSheet(concatenated, droppedAt);
    for (auto& dropped : droppedAt) {
      ctx.warn("subset:unsupported-at-rule",
               "html: at-rule '" + dropped + "' dropped (not in subset)", nullptr);
    }
    for (auto& rule : rules) {
      auto selectors = pagx::detail::SplitTopLevelCommas(rule.selectors);
      PropertyMap decls;
      ParseStyleString(rule.declarations, decls);
      if (decls.empty()) continue;
      for (auto& selRaw : selectors) {
        std::string sel = Trim(selRaw);
        if (sel.empty()) continue;
        auto parsed = css_cascade::ClassifySelector(sel);
        switch (parsed.kind) {
          case css_cascade::SelectorKind::Class:
            ctx.addClassRule(parsed.key, decls);
            break;
          case css_cascade::SelectorKind::Element:
            ctx.addElementRule(parsed.key, decls);
            break;
          case css_cascade::SelectorKind::Universal:
            ctx.warn("subset:unsupported-selector",
                     "html: universal selector '*' is not allowed; rule dropped", nullptr);
            break;
          case css_cascade::SelectorKind::Unsupported:
            ctx.warn("subset:unsupported-selector",
                     "html: selector '" + sel + "' is not supported; rule dropped", nullptr);
            break;
        }
      }
    }
  }

  // Remove <style> elements unless preservation is requested.
  if (!ctx.options().preserveStyleBlock) {
    TransformChildren(head, [&](const std::shared_ptr<DOMNode>&,
                                const std::shared_ptr<DOMNode>&,
                                const std::shared_ptr<DOMNode>& c) -> std::shared_ptr<DOMNode> {
      if (c->type == DOMNodeType::Element && c->name == "style") return nullptr;
      return c;
    });
  }
}

//==================================================================================================
// ComputedStylePass
//==================================================================================================

namespace {

void ResolveElement(const std::shared_ptr<DOMNode>& element, const PropertyMap& inherited,
                    HTMLTransformContext& ctx);

void ResolveTree(const std::shared_ptr<DOMNode>& element, const PropertyMap& inheritedIn,
                 HTMLTransformContext& ctx) {
  if (!element || element->type != DOMNodeType::Element) return;
  PropertyMap inherited = inheritedIn;
  // Compute this element's resolved style and update `inherited` for descendants.
  if (element->name != "html" && element->name != "head" && element->name != "title" &&
      element->name != "meta" && element->name != "style") {
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
    ResolveTree(child, inherited, ctx);
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
  ctx.setResolved(element.get(), std::move(resolved));
}

}  // namespace

void ComputedStylePass::apply(const std::shared_ptr<DOMNode>& root, HTMLTransformContext& ctx) {
  if (!root || ctx.hasFatal()) return;
  auto body = root->getFirstChild("body");
  if (!body) return;
  PropertyMap empty;
  ResolveTree(body, empty, ctx);
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

  const auto& table = subset_props::SubsetPropertyTable();
  PropertyMap filtered;

  // Phase 1: resolve `font-size` first using the PARENT's font-size as the em base. CSS spec:
  // `font-size: 2em` resolves against the parent's computed font-size, not the element's own.
  float ownFontSizePx = parentFontSizePx;
  auto fsIt = resolved->find("font-size");
  if (fsIt != resolved->end()) {
    subset_props::PropertyContext pctx = {};
    pctx.propertyName = "font-size";
    pctx.ownerTag = element->name;
    pctx.canvasWidth = ctx.canvasWidth();
    pctx.canvasHeight = ctx.canvasHeight();
    pctx.currentFontSizePx = parentFontSizePx;
    std::string newValue = subset_props::ResolveLength(fsIt->second, pctx, ctx);
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
      case subset_props::PropAction::Keep:
        filtered[propName] = Trim(propValue);
        break;
      case subset_props::PropAction::Drop: {
        std::string msg = handler.dropMessage ? handler.dropMessage : "is not in the subset";
        ctx.warn("subset:unsupported-property",
                 "html: '" + propName + ": " + propValue + "' " + msg, element);
        break;
      }
      case subset_props::PropAction::Transform: {
        subset_props::PropertyContext pctx = {};
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
                HTMLTransformContext& ctx) {
  if (!element || element->type != DOMNodeType::Element) return;
  float fontSizeForChildren = parentFontSizePx;
  if (element->name != "html" && element->name != "head" && element->name != "title" &&
      element->name != "meta" && element->name != "style") {
    fontSizeForChildren = FilterElement(element, parentFontSizePx, ctx);
  }
  // SVG subtrees are opaque to property filtering.
  if (element->name == "svg") return;
  auto child = element->firstChild;
  while (child) {
    FilterTree(child, fontSizeForChildren, ctx);
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
  FilterTree(body, 14.0f, ctx);
}

//==================================================================================================
// StructureNormalizationPass
//==================================================================================================

namespace {

// Returns true when `text` consists entirely of ASCII whitespace.
bool IsBlankText(const std::string& text) {
  for (char c : text) {
    if (!std::isspace(static_cast<unsigned char>(c))) return false;
  }
  return true;
}

// Wraps non-blank text children of a container into a synthetic `<p>` (or `<span>` when the
// parent is itself a text leaf). Element children are left untouched.
void WrapStrayText(const std::shared_ptr<DOMNode>& parent, HTMLTransformContext& ctx,
                   bool insideTextLeaf) {
  std::string wrapperTag = insideTextLeaf ? "span" : "p";
  std::shared_ptr<DOMNode> prev = nullptr;
  auto child = parent->firstChild;
  while (child) {
    auto next = child->nextSibling;
    if (child->type == DOMNodeType::Text && !IsBlankText(child->name)) {
      // Wrap this text node.
      auto wrapper = MakeElement(wrapperTag);
      wrapper->firstChild = child;
      child->nextSibling = nullptr;
      wrapper->nextSibling = next;
      if (prev) {
        prev->nextSibling = wrapper;
      } else {
        parent->firstChild = wrapper;
      }
      ctx.warn("subset:text-wrapped",
               "html: stray text inside <" + parent->name + "> wrapped in <" + wrapperTag + ">",
               parent);
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
                       bool parentIsTextLeaf) {
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
      bool childIsTextLeaf = IsTextLeafTag(child->name);
      NormalizeChildren(child, ctx, childIsTextLeaf);
    }
    prev = child;
    child = next;
  }
  // After cleaning element children, wrap stray text. Skip inline-run parents (<span>, <a>,
  // <br>) since their text content is meaningful.
  if (parent->type == DOMNodeType::Element && parent->name != "br" && parent->name != "img" &&
      parent->name != "svg" && parent->name != "title" && parent->name != "meta" &&
      parent->name != "style") {
    bool wrapAsSpan = parentIsTextLeaf || IsInlineRunTag(parent->name);
    if (IsContainerTag(parent->name)) {
      wrapAsSpan = false;
    }
    if (!wrapAsSpan && IsContainerTag(parent->name)) {
      WrapStrayText(parent, ctx, /*insideTextLeaf=*/false);
    } else if (parentIsTextLeaf) {
      // Text leaves keep their text children unmodified (they are valid inside <p>/<hN>).
    }
  }
}

}  // namespace

void StructureNormalizationPass::apply(const std::shared_ptr<DOMNode>& root,
                                       HTMLTransformContext& ctx) {
  if (!root || ctx.hasFatal()) return;
  auto body = root->getFirstChild("body");
  if (!body) return;
  NormalizeChildren(body, ctx, /*parentIsTextLeaf=*/false);
}

//==================================================================================================
// InlineStyleEmitterPass
//==================================================================================================

namespace {

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
    element->attributes.erase(
        std::remove_if(element->attributes.begin(), element->attributes.end(),
                       [](const DOMAttribute& a) { return a.name == "style" && a.value.empty(); }),
        element->attributes.end());
  }
}

void EmitTree(const std::shared_ptr<DOMNode>& element, HTMLTransformContext& ctx) {
  if (!element || element->type != DOMNodeType::Element) return;
  if (element->name != "html" && element->name != "head" && element->name != "title" &&
      element->name != "meta" && element->name != "style") {
    EmitStyleAttribute(element, ctx);
    if (!ctx.options().preserveClassAttribute) {
      element->attributes.erase(
          std::remove_if(element->attributes.begin(), element->attributes.end(),
                         [](const DOMAttribute& a) { return a.name == "class"; }),
          element->attributes.end());
    }
  }
  // SVG subtrees are opaque; their attributes are forwarded verbatim by the importer.
  if (element->name == "svg") return;
  auto child = element->firstChild;
  while (child) {
    EmitTree(child, ctx);
    child = child->nextSibling;
  }
}

}  // namespace

void InlineStyleEmitterPass::apply(const std::shared_ptr<DOMNode>& root,
                                   HTMLTransformContext& ctx) {
  if (!root || ctx.hasFatal()) return;
  auto body = root->getFirstChild("body");
  if (!body) return;
  EmitTree(body, ctx);
  (void)IsVoidElement;  // referenced for future void-element handling; keep helper alive.
}

}  // namespace pagx::html_passes
