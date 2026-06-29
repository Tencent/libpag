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

#include "pagx/html/importer/HTMLInlineSvgEmitter.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <unordered_set>
#include <vector>
#include "pagx/html/HTMLWriter.h"
#include "pagx/html/importer/HTMLBoxAttributes.h"
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/types/Color.h"
#include "pagx/xml/XMLDOM.h"

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
// effective colour via the CSS cascade and passes that as `inheritedColor`. We
// deliberately skip re-reading the root's own `color` / `style` to preserve the
// caller-supplied formatting (e.g. so a snapshot-stage `color: rgb(...)`
// round-trips as the hex form we resolved from the parsed `Color`). Descendants
// get the standard cascade tracking.
//
// This runs before serialising the SVG into a PAGX import directive because
// the downstream SVG importer has no notion of `color` / `currentColor` — by
// the time it parses the directive content, every fill/stroke must already
// carry a concrete colour token.
void ResolveCurrentColorRecursive(const std::shared_ptr<DOMNode>& node,
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
    ResolveCurrentColorRecursive(child, here, depth + 1, /*isRoot=*/false);
  }
}

// Pulls the fragment id out of a `url(#id)` functional value (with optional quotes and
// surrounding whitespace), returning empty when `value` is not such a reference. SVG paint
// servers, clip-path, mask, filter, and marker properties all use this `url(#…)` spelling.
std::string ExtractUrlFragmentId(const std::string& value) {
  std::string trimmed = Trim(value);
  if (trimmed.size() < 6) return {};
  if (trimmed.compare(0, 4, "url(") != 0 || trimmed.back() != ')') return {};
  std::string inner = Trim(trimmed.substr(4, trimmed.size() - 5));
  if (inner.size() >= 2 && (inner.front() == '"' || inner.front() == '\'') &&
      inner.back() == inner.front()) {
    inner = Trim(inner.substr(1, inner.size() - 2));
  }
  if (inner.empty() || inner.front() != '#') return {};
  return inner.substr(1);
}

// Pulls the fragment id out of a bare `#id` local reference (used by `href` / `xlink:href`,
// e.g. a `<use href="#shape">` or a gradient inheriting stops via `href="#base">`). Returns
// empty for non-local references (external URLs) or malformed values.
std::string ExtractHashFragmentId(const std::string& value) {
  std::string trimmed = Trim(value);
  if (trimmed.size() < 2 || trimmed.front() != '#') return {};
  return trimmed.substr(1);
}

// Records every id this single node references through a `url(#…)` paint/effect property, a
// `href` / `xlink:href` local link, or an inline `style="…: url(#…)"` declaration.
void CollectNodeReferences(const std::shared_ptr<DOMNode>& node,
                           std::unordered_set<std::string>& out) {
  if (!node || node->type != DOMNodeType::Element) return;
  for (const auto& attr : node->attributes) {
    if (EqualsIgnoreCase(attr.name, "href") || EqualsIgnoreCase(attr.name, "xlink:href")) {
      std::string id = ExtractHashFragmentId(attr.value);
      if (!id.empty()) out.insert(id);
      continue;
    }
    if (EqualsIgnoreCase(attr.name, "style")) {
      for (const auto& decl : ParseStyleDeclarations(attr.value)) {
        std::string id = ExtractUrlFragmentId(decl.second);
        if (!id.empty()) out.insert(id);
      }
      continue;
    }
    std::string id = ExtractUrlFragmentId(attr.value);
    if (!id.empty()) out.insert(id);
  }
}

// Walks the subtree rooted at `node`, unioning every referenced id (see `CollectNodeReferences`)
// and every id this subtree itself defines, so the caller can compute which references point
// outside the subtree.
void CollectSubtreeReferencesAndIds(const std::shared_ptr<DOMNode>& node,
                                    std::unordered_set<std::string>& referenced,
                                    std::unordered_set<std::string>& defined, int depth) {
  if (!node || depth >= MAX_HTML_RECURSION_DEPTH) return;
  if (node->type != DOMNodeType::Element) return;
  CollectNodeReferences(node, referenced);
  if (auto* idAttr = node->findAttribute("id")) {
    if (!idAttr->empty()) defined.insert(*idAttr);
  }
  for (auto child = node->getFirstChild(); child; child = child->getNextSibling()) {
    CollectSubtreeReferencesAndIds(child, referenced, defined, depth + 1);
  }
}

// Indexes every id-bearing element under any `<defs>` in the subtree into `table`. A first writer
// wins, matching browser id-resolution where the first element with a given id is authoritative.
void IndexDefsRecursive(const std::shared_ptr<DOMNode>& node,
                        std::unordered_map<std::string, std::shared_ptr<DOMNode>>& table,
                        bool insideDefs, int depth) {
  if (!node || depth >= MAX_HTML_RECURSION_DEPTH) return;
  if (node->type != DOMNodeType::Element) return;
  bool nowInsideDefs = insideDefs || EqualsIgnoreCase(node->name, "defs");
  if (insideDefs) {
    if (auto* idAttr = node->findAttribute("id")) {
      if (!idAttr->empty()) table.emplace(*idAttr, node);
    }
  }
  for (auto child = node->getFirstChild(); child; child = child->getNextSibling()) {
    IndexDefsRecursive(child, table, nowInsideDefs, depth + 1);
  }
}

}  // namespace

void HTMLInlineSvgEmitter::resolveCurrentColor(const std::shared_ptr<DOMNode>& svgRoot,
                                               const std::string& rootColor) {
  ResolveCurrentColorRecursive(svgRoot, rootColor, /*depth=*/0, /*isRoot=*/true);
}

void HTMLInlineSvgEmitter::collectSharedDefs(const std::shared_ptr<DOMNode>& root) {
  IndexDefsRecursive(root, _sharedDefs, /*insideDefs=*/false, /*depth=*/0);
}

void HTMLInlineSvgEmitter::injectReferencedDefs(const std::shared_ptr<DOMNode>& svgRoot) {
  if (!svgRoot || _sharedDefs.empty()) return;

  std::unordered_set<std::string> referenced;
  std::unordered_set<std::string> defined;
  CollectSubtreeReferencesAndIds(svgRoot, referenced, defined, /*depth=*/0);

  // Resolve the transitive closure of external references: a referenced gradient may itself
  // inherit stops from another shared def via `href="#base"`, so cloning one def can pull in
  // more. Worklist over ids that resolve to a shared def and are not already defined locally.
  std::vector<std::shared_ptr<DOMNode>> toInject;
  std::unordered_set<std::string> queued;
  std::vector<std::string> worklist(referenced.begin(), referenced.end());
  while (!worklist.empty()) {
    std::string id = worklist.back();
    worklist.pop_back();
    if (defined.count(id) > 0 || queued.count(id) > 0) continue;
    auto it = _sharedDefs.find(id);
    if (it == _sharedDefs.end()) continue;
    queued.insert(id);
    auto clone = CloneDOMSubtree(it->second, /*depth=*/0);
    if (!clone) continue;
    toInject.push_back(clone);
    std::unordered_set<std::string> nested;
    CollectSubtreeReferencesAndIds(it->second, nested, defined, /*depth=*/0);
    for (const auto& nestedId : nested) {
      if (defined.count(nestedId) == 0 && queued.count(nestedId) == 0) {
        worklist.push_back(nestedId);
      }
    }
  }
  if (toInject.empty()) return;

  auto defs = std::make_shared<DOMNode>();
  defs->name = "defs";
  defs->type = DOMNodeType::Element;
  std::shared_ptr<DOMNode> lastChild = nullptr;
  for (auto& node : toInject) {
    if (lastChild) {
      lastChild->nextSibling = node;
    } else {
      defs->firstChild = node;
    }
    lastChild = node;
  }
  defs->nextSibling = svgRoot->firstChild;
  svgRoot->firstChild = defs;
}

std::string HTMLInlineSvgEmitter::serialize(const std::shared_ptr<DOMNode>& svgNode) {
  std::string out;
  SerializeNode(out, svgNode, 0);
  return out;
}

// SVG `fill` / `stroke` attributes accept CSS colour tokens. PAGX's SVG importer
// understands `#RRGGBB`, `#RRGGBBAA`, and `rgb()/rgba()`, but not the CSS `color()`
// function used for DisplayP3 — `ColorToSVGHex` collapses those to sRGB hex for us.
// Alpha < 1 is rare for `currentColor` resolution (CSS `color` rarely carries
// transparency), but we round-trip it via the 8-digit hex form when present so the
// importer's `addFillStroke` path picks up the right alpha.
std::string HTMLInlineSvgEmitter::formatColorForAttribute(const Color& color) {
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

}  // namespace pagx
