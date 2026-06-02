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

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "pagx/html/importer/HTMLBoxAttributes.h"

namespace pagx {

struct DOMNode;
class HTMLDiagnosticSink;
class HTMLValueParser;

/**
 * Owns the importer's CSS cascade: rule tables built from `<style>` blocks, the per-element
 * resolved-style cache, the inherited-style computation that walks the DOM cascade, and the
 * box-attribute parser that maps a resolved property map into the typed `HTMLBoxAttributes`
 * the layer builder consumes.
 *
 * The cascade borrows the importer's diagnostic sink and value parser; both are populated
 * before any cascade method is called. Concrete font-family chains discovered while
 * resolving inherited style are forwarded through `_fontFallbackSink` (set up by
 * `HTMLParserContext`) so the parser context can union them into the document-wide
 * `FontConfig` fallback list.
 */
class HTMLStyleCascade {
 public:
  using PropertyMap = std::unordered_map<std::string, std::string>;
  // Receives concrete font-family chains. `userData` is the pointer registered alongside the
  // thunk via `setFontFallbackSink`. The function-pointer + opaque-pointer shape avoids
  // pulling `<functional>` into the cascade header and lets the caller wire a member sink
  // without `std::bind`.
  using FontFallbackThunk = void (*)(void* userData, const std::vector<std::string>& chain);

  HTMLStyleCascade(HTMLDiagnosticSink& sink, HTMLValueParser& valueParser);

  /** Wires the callback that receives concrete font-family chains discovered by
   *  `resolveInheritedStyle`. Caller pools them for FontConfig fallback registration. */
  void setFontFallbackSink(FontFallbackThunk thunk, void* userData);

  /** Walks `<head>` and populates the class / element rule tables from `<style>` blocks. */
  void collectStyles(const std::shared_ptr<DOMNode>& head);

  /** Returns the resolved property map for `node`, computing and caching it on first call.
   *  Priority: element defaults → element-rule cascade → class-rule cascade → inline `style`. */
  const PropertyMap& getResolvedStyle(const std::shared_ptr<DOMNode>& node);

  /** Returns the value of `property` from the resolved style, or `fallback` when absent. */
  std::string getStyleProperty(const std::shared_ptr<DOMNode>& node, const std::string& property,
                               const std::string& fallback = "");

  /** Computes the inherited style for `element` based on `parent`. Mirrors the CSS cascade for
   *  text-related properties and pre-resolves the numeric forms (font-size, letter-spacing,
   *  resolved text colour) so text-leaf conversion can read them without re-parsing. */
  HTMLInheritedStyle resolveInheritedStyle(const std::shared_ptr<DOMNode>& element,
                                           const HTMLInheritedStyle& parent);

  /** Resolves the box-model attributes from the element's resolved style (sizing, positioning,
   *  layout, visuals, transform). */
  HTMLBoxAttributes computeBoxAttributes(const std::shared_ptr<DOMNode>& element);

 private:
  void parseStyleBlock(const std::shared_ptr<DOMNode>& styleNode);
  void mergeClassRules(const std::string& classAttribute, PropertyMap& out);

  void parseBoxSizing(HTMLBoxAttributes& box, const PropertyMap& props);
  void parseBoxPositioning(HTMLBoxAttributes& box, const PropertyMap& props);
  void parseBoxLayout(HTMLBoxAttributes& box, const PropertyMap& props);
  void parseBoxVisuals(HTMLBoxAttributes& box, const PropertyMap& props);
  void parseBoxTransform(HTMLBoxAttributes& box, const PropertyMap& props);

  void parseBorderRadius(HTMLBoxAttributes& box, const PropertyMap& props);
  void parseBorder(HTMLBoxAttributes& box, const std::string& border);

  // `parseBoxLayout` per-side margin longhand handler. Hoisted out of the original lambda so
  // the project's "no lambda" rule is honoured.
  void applyMarginLonghand(const PropertyMap& props, const char* propName, float& outPx);

  HTMLDiagnosticSink& _diagnostics;
  HTMLValueParser& _valueParser;
  FontFallbackThunk _fontFallbackThunk = nullptr;
  void* _fontFallbackUserData = nullptr;

  // CSS class selectors (key = class name without the dot). Pre-parsed at <style>-collection
  // time so per-element resolution can copy entries instead of re-running ParseStyleString
  // for every node referencing the same class.
  std::unordered_map<std::string, PropertyMap> _classRules = {};

  // CSS element selectors (key = lower-case tag name). Same shape as `_classRules`.
  std::unordered_map<std::string, PropertyMap> _elementRules = {};

  // Cached resolved style per DOM node. Each entry merges inline + class + element rules + tag
  // defaults.
  std::unordered_map<const DOMNode*, PropertyMap> _resolvedCache = {};
};

}  // namespace pagx
