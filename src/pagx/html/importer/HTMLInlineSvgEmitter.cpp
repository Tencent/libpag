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
#include <cstdlib>
#include <unordered_set>
#include <vector>
#include "pagx/html/HTMLWriter.h"
#include "pagx/html/importer/HTMLBoxAttributes.h"
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLValueParser.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/RadialGradient.h"
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

// Rebuilds an inline `style="..."` value with every `opacity` declaration removed, returning the
// remaining declarations re-joined. Used to drop the root `<svg>`'s own opacity once it has been
// hoisted onto the enclosing layer's alpha, so the downstream SVG importer does not apply it again.
std::string RemoveOpacityFromInlineStyle(const std::string& style) {
  auto decls = ParseStyleDeclarations(style);
  std::string out;
  out.reserve(style.size());
  bool first = true;
  for (auto& decl : decls) {
    if (EqualsIgnoreCase(decl.first, "opacity")) continue;
    if (!first) out.push_back(';');
    out += decl.first;
    out.push_back(':');
    out.push_back(' ');
    out += decl.second;
    first = false;
  }
  return out;
}

constexpr float kInlineSvgPi = 3.14159265358979323846f;

// Returns the `background` / `background-image` value from a `<div>`'s inline `style`, or empty
// when the div carries no background paint. `background` wins over `background-image` on a tie
// (last-writer semantics are irrelevant here — the exporter only ever writes one).
std::string ExtractBackgroundPaint(const std::string& style) {
  std::string paint;
  for (const auto& decl : ParseStyleDeclarations(style)) {
    if (EqualsIgnoreCase(decl.first, "background") ||
        EqualsIgnoreCase(decl.first, "background-image")) {
      if (!decl.second.empty()) paint = Trim(decl.second);
    }
  }
  return paint;
}

// Linearly interpolates a gradient's stop list (assumed sorted by offset, sRGB channels) at
// position `t` in [0, 1]. Clamps to the endpoint colours outside the stop range.
Color InterpolateStops(const std::vector<ColorStop*>& stops, float t) {
  if (stops.empty()) return {0, 0, 0, 1, ColorSpace::SRGB};
  if (t <= stops.front()->offset) return stops.front()->color;
  if (t >= stops.back()->offset) return stops.back()->color;
  for (size_t i = 1; i < stops.size(); ++i) {
    if (t > stops[i]->offset) continue;
    const Color& a = stops[i - 1]->color;
    const Color& b = stops[i]->color;
    float span = stops[i]->offset - stops[i - 1]->offset;
    float f = span > 1e-6f ? (t - stops[i - 1]->offset) / span : 0.0f;
    Color c = {};
    c.red = a.red + (b.red - a.red) * f;
    c.green = a.green + (b.green - a.green) * f;
    c.blue = a.blue + (b.blue - a.blue) * f;
    c.alpha = a.alpha + (b.alpha - a.alpha) * f;
    c.colorSpace = ColorSpace::SRGB;
    return c;
  }
  return stops.back()->color;
}

// Averages every stop colour, used as the neutral fallback when a conic's centre coincides with
// the sampling point (the sweep angle is undefined there).
Color AverageStops(const std::vector<ColorStop*>& stops) {
  if (stops.empty()) return {0, 0, 0, 1, ColorSpace::SRGB};
  Color c = {0, 0, 0, 0, ColorSpace::SRGB};
  for (const auto* stop : stops) {
    c.red += stop->color.red;
    c.green += stop->color.green;
    c.blue += stop->color.blue;
    c.alpha += stop->color.alpha;
  }
  float n = static_cast<float>(stops.size());
  c.red /= n;
  c.green /= n;
  c.blue /= n;
  c.alpha /= n;
  return c;
}

// Resolves a single CSS `<length-percentage>` token against `axis` (percentages are box-relative;
// bare numbers / `px` are absolute). Other units are treated as px, which is all the exporter
// writes for a conic centre.
float ResolveConicAxis(const std::string& token, float axis) {
  float fraction = NAN;
  if (ParseCssPercentage(token, fraction)) return fraction * axis;
  return std::strtof(token.c_str(), nullptr);
}

// Parses the `at <cx> <cy>` centre from a `conic-gradient(...)` value into box-local pixels,
// defaulting to the box centre (CSS default `at 50% 50%`) when absent.
void ParseConicCenter(const std::string& value, float boxW, float boxH, float& cx, float& cy) {
  cx = boxW * 0.5f;
  cy = boxH * 0.5f;
  std::string args = ExtractParenArgs(value);
  if (args.empty()) return;
  auto parts = SplitTopLevelCommas(args);
  if (parts.empty()) return;
  auto tokens = SplitTopLevelWhitespace(parts[0]);
  for (size_t i = 0; i + 2 < tokens.size() + 1 && i < tokens.size(); ++i) {
    if (ToLower(tokens[i]) != "at") continue;
    if (i + 1 < tokens.size()) cx = ResolveConicAxis(tokens[i + 1], boxW);
    if (i + 2 < tokens.size()) cy = ResolveConicAxis(tokens[i + 2], boxH);
    break;
  }
}

// Sets (or overwrites, case-insensitively) attribute `name` on `node` to `value`.
void SetAttribute(const std::shared_ptr<DOMNode>& node, const char* name, const std::string& value) {
  for (auto& attr : node->attributes) {
    if (EqualsIgnoreCase(attr.name, name)) {
      attr.value = value;
      return;
    }
  }
  DOMAttribute added;
  added.name = name;
  added.value = value;
  node->attributes.push_back(added);
}

// Samples a conic gradient at the centre of its `boxW x boxH` box. `cssStartDeg` is the CSS
// `from` angle (0deg = up, clockwise); `cx`/`cy` are the sweep centre in box-local pixels. The
// exporter's conic-stroke centre sits far outside each small shape box, so the sweep varies only
// slightly across the box and this single sample reproduces the browser's near-uniform hue.
Color SampleConicOverBox(const std::vector<ColorStop*>& stops, float cssStartDeg, float cx,
                         float cy, float boxW, float boxH) {
  float dx = boxW * 0.5f - cx;
  float dy = boxH * 0.5f - cy;
  if (std::abs(dx) < 1e-4f && std::abs(dy) < 1e-4f) return AverageStops(stops);
  float angle = std::atan2(dx, -dy) * 180.0f / kInlineSvgPi;
  angle -= cssStartDeg;
  angle = std::fmod(angle, 360.0f);
  if (angle < 0.0f) angle += 360.0f;
  return InterpolateStops(stops, angle / 360.0f);
}

}  // namespace

void HTMLInlineSvgEmitter::stripRootOpacity(const std::shared_ptr<DOMNode>& svgRoot) {
  if (!svgRoot || svgRoot->type != DOMNodeType::Element) return;
  std::vector<DOMAttribute> kept;
  kept.reserve(svgRoot->attributes.size());
  for (auto& attr : svgRoot->attributes) {
    if (EqualsIgnoreCase(attr.name, "opacity")) continue;
    if (EqualsIgnoreCase(attr.name, "style")) {
      std::string rewritten = RemoveOpacityFromInlineStyle(attr.value);
      if (rewritten.empty()) continue;
      attr.value = rewritten;
    }
    kept.push_back(attr);
  }
  svgRoot->attributes = std::move(kept);
}

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

void HTMLInlineSvgEmitter::reconstructForeignObjectPaints(const std::shared_ptr<DOMNode>& svgRoot,
                                                          HTMLValueParser& valueParser) {
  reconstructForeignObjectsRecursive(svgRoot, valueParser, /*depth=*/0);
}

void HTMLInlineSvgEmitter::reconstructForeignObjectsRecursive(const std::shared_ptr<DOMNode>& node,
                                                              HTMLValueParser& valueParser,
                                                              int depth) {
  if (!node || depth >= MAX_HTML_RECURSION_DEPTH) return;
  if (node->type != DOMNodeType::Element) return;

  std::shared_ptr<DOMNode> prev = nullptr;
  auto child = node->firstChild;
  while (child) {
    std::shared_ptr<DOMNode> replacement = nullptr;
    if (child->type == DOMNodeType::Element && EqualsIgnoreCase(child->name, "foreignObject")) {
      replacement = buildForeignObjectReplacement(child, valueParser);
    }
    if (replacement) {
      replacement->nextSibling = child->nextSibling;
      if (prev) {
        prev->nextSibling = replacement;
      } else {
        node->firstChild = replacement;
      }
      prev = replacement;
      child = replacement->nextSibling;
    } else {
      reconstructForeignObjectsRecursive(child, valueParser, depth + 1);
      prev = child;
      child = child->nextSibling;
    }
  }
}

std::shared_ptr<DOMNode> HTMLInlineSvgEmitter::buildForeignObjectReplacement(
    const std::shared_ptr<DOMNode>& foreignObject, HTMLValueParser& valueParser) {
  // The div child carries the paint as a CSS background; without one there is nothing to recover.
  std::shared_ptr<DOMNode> div = nullptr;
  for (auto c = foreignObject->getFirstChild(); c; c = c->getNextSibling()) {
    if (c->type == DOMNodeType::Element) {
      div = c;
      break;
    }
  }
  if (!div) return nullptr;
  const auto* styleAttr = div->findAttribute("style");
  if (!styleAttr) return nullptr;
  std::string paint = ExtractBackgroundPaint(*styleAttr);
  if (paint.empty()) return nullptr;

  const auto* wAttr = foreignObject->findAttribute("width");
  const auto* hAttr = foreignObject->findAttribute("height");
  float boxW = wAttr ? std::strtof(wAttr->c_str(), nullptr) : 0.0f;
  float boxH = hAttr ? std::strtof(hAttr->c_str(), nullptr) : 0.0f;

  // Resolve the div's background to a single concrete colour: solid verbatim, gradient sampled to
  // its representative hue over the box. `url(...)` image backgrounds are left for the existing
  // path (returning nullptr keeps the node untouched).
  std::string lowered = ToLower(paint);
  Color color = {0, 0, 0, 1, ColorSpace::SRGB};
  if (lowered.compare(0, 15, "conic-gradient(") == 0) {
    auto* grad = valueParser.parseConicGradient(paint);
    if (!grad || grad->colorStops.empty()) return nullptr;
    float cx = NAN;
    float cy = NAN;
    ParseConicCenter(paint, boxW, boxH, cx, cy);
    color = SampleConicOverBox(grad->colorStops, grad->startAngle + 90.0f, cx, cy, boxW, boxH);
  } else if (lowered.compare(0, 16, "linear-gradient(") == 0) {
    auto* grad = valueParser.parseLinearGradient(paint, boxW, boxH);
    if (!grad || grad->colorStops.empty()) return nullptr;
    color = InterpolateStops(grad->colorStops, 0.5f);
  } else if (lowered.compare(0, 16, "radial-gradient(") == 0) {
    auto* grad = valueParser.parseRadialGradient(paint, boxW, boxH);
    if (!grad || grad->colorStops.empty()) return nullptr;
    color = InterpolateStops(grad->colorStops, 0.5f);
  } else if (lowered.find("url(") != std::string::npos) {
    return nullptr;
  } else {
    color = valueParser.parseColor(paint);
  }
  std::string colorHex = formatColorForAttribute(color);

  // With a mask, the masked region is exactly the mask's stroke shapes — rebuild them directly and
  // recolour their stroke, dropping the foreignObject + mask indirection entirely.
  const auto* maskAttr = foreignObject->findAttribute("mask");
  std::string maskId = maskAttr ? ExtractUrlFragmentId(*maskAttr) : std::string();
  auto maskNode = maskId.empty() ? nullptr : lookupSharedDef(maskId);
  if (maskNode) {
    std::vector<std::shared_ptr<DOMNode>> shapes;
    for (auto c = maskNode->getFirstChild(); c; c = c->getNextSibling()) {
      if (c->type != DOMNodeType::Element) continue;
      auto clone = CloneDOMSubtree(c, /*depth=*/0);
      if (!clone) continue;
      SetAttribute(clone, "stroke", colorHex);
      shapes.push_back(clone);
    }
    if (shapes.empty()) return nullptr;
    const auto* transform = foreignObject->findAttribute("transform");
    if (shapes.size() == 1 && !transform) {
      return shapes.front();
    }
    auto group = std::make_shared<DOMNode>();
    group->name = "g";
    group->type = DOMNodeType::Element;
    if (transform) SetAttribute(group, "transform", *transform);
    std::shared_ptr<DOMNode> last = nullptr;
    for (auto& shape : shapes) {
      if (last) {
        last->nextSibling = shape;
      } else {
        group->firstChild = shape;
      }
      last = shape;
    }
    return group;
  }

  // No mask: the foreignObject was a plain painted box. Emit a solid-filled <rect> so it renders a
  // representative colour instead of the SVG importer's fallback black.
  const auto* xAttr = foreignObject->findAttribute("x");
  const auto* yAttr = foreignObject->findAttribute("y");
  auto rect = std::make_shared<DOMNode>();
  rect->name = "rect";
  rect->type = DOMNodeType::Element;
  SetAttribute(rect, "x", xAttr ? *xAttr : "0");
  SetAttribute(rect, "y", yAttr ? *yAttr : "0");
  if (wAttr) SetAttribute(rect, "width", *wAttr);
  if (hAttr) SetAttribute(rect, "height", *hAttr);
  SetAttribute(rect, "fill", colorHex);
  const auto* transform = foreignObject->findAttribute("transform");
  if (transform) SetAttribute(rect, "transform", *transform);
  return rect;
}

std::string HTMLInlineSvgEmitter::serialize(const std::shared_ptr<DOMNode>& svgNode) {
  std::string out;
  SerializeNode(out, svgNode, 0);
  return out;
}

std::shared_ptr<DOMNode> HTMLInlineSvgEmitter::lookupSharedDef(const std::string& id) const {
  auto it = _sharedDefs.find(id);
  return it != _sharedDefs.end() ? it->second : nullptr;
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
