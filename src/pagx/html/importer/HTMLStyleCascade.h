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
#include "pagx/html/importer/HTMLCssCascade.h"

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
 * before any cascade method is called. Font-family chains remain scoped to inherited styles.
 */
class HTMLStyleCascade {
 public:
  using PropertyMap = std::unordered_map<std::string, std::string>;
  // Reports whether a concrete family resolves to a typeface whose family name matches — i.e. a
  // registered/embedded font or a genuinely installed system font, as opposed to a name the
  // renderer silently substitutes with a mismatched default. Used to choose the first *resolvable*
  // family from a CSS font-family stack as `primaryFontFamily`. The callback uses an opaque
  // user-data pointer so the cascade stays free of `<functional>`.
  using FontAvailabilityThunk = bool (*)(void* userData, const std::string& family);

  // Rewrites a resolved (family, style) pair in place to the names the platform resolves it to, so
  // the pair written to `Text::fontFamily` / `Text::fontStyle` can be looked up by name outside
  // this process — an editor consuming the exported PAGX resolves fonts with its own, usually
  // exact-matching, font lookup. Same function-pointer + opaque-user-data shape as the sinks above.
  using FontFaceNameThunk = void (*)(void* userData, std::string& family, std::string& style,
                                    const CSSFontStyle& request);

  HTMLStyleCascade(HTMLDiagnosticSink& sink, HTMLValueParser& valueParser);

  /** Wires the predicate that reports whether a concrete family resolves to a matching typeface.
   *  When unset, `resolveInheritedStyle` keeps the stack's first concrete family verbatim. */
  void setFontAvailabilitySink(FontAvailabilityThunk thunk, void* userData);

  /** Wires the in-place normaliser mapping a resolved pair to the platform's own spelling.
   *  When unset, `resolveInheritedStyle` keeps both names verbatim. */
  void setFontFaceNameSink(FontFaceNameThunk thunk, void* userData);

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
   *  resolved text colour) so text-leaf conversion can read them without re-parsing.
   */
  HTMLInheritedStyle resolveInheritedStyle(const std::shared_ptr<DOMNode>& element,
                                           const HTMLInheritedStyle& parent);

  /** Resolves the box-model attributes from the element's resolved style (sizing, positioning,
   *  layout, visuals, transform). */
  HTMLBoxAttributes computeBoxAttributes(const std::shared_ptr<DOMNode>& element);

  // Re-resolves size-dependent corner geometry after percentage/flex layout.
  void resolveUsedBoxSize(HTMLBoxAttributes& box, const std::shared_ptr<DOMNode>& element,
                          float width, float height);

  /** Returns the `@keyframes` rules collected from `<style>` blocks, keyed by keyframes name.
   *  Populated by `collectStyles`; consumed by the animation builder (see §13). */
  const std::unordered_map<std::string, html::CssKeyframesRule>& keyframes() const {
    return _keyframes;
  }

 private:
  void parseStyleBlock(const std::shared_ptr<DOMNode>& styleNode);
  void mergeClassRules(const std::string& classAttribute, PropertyMap& out);

  void parseBoxSizing(HTMLBoxAttributes& box, const PropertyMap& props);
  void parseBoxPositioning(HTMLBoxAttributes& box, const PropertyMap& props);
  void parseBoxLayout(HTMLBoxAttributes& box, const PropertyMap& props);
  void parseBoxMargin(HTMLBoxAttributes& box, const PropertyMap& props);
  void parseBoxVisuals(HTMLBoxAttributes& box, const PropertyMap& props,
                        bool deferBorderRadius = false);
  void parseBoxTransform(HTMLBoxAttributes& box, const PropertyMap& props);

  void parseBorderRadius(HTMLBoxAttributes& box, const PropertyMap& props);
  void parseBorder(HTMLBoxAttributes& box, const std::string& border);

  // Resolves the element's background paint colour — the `background-color` longhand, falling
  // back to a colour-only `background` shorthand — into `out`. Returns false when no colour
  // layer is declared, or when the value is a gradient / url image rather than a colour.
  bool resolveBackgroundColor(const PropertyMap& props, Color* out);

  // Resolves `-webkit-text-stroke` (shorthand or longhands) into `out.textStrokeWidthPx` /
  // `out.textStrokeColor`. Called at the tail of `resolveInheritedStyle` once the resolved text
  // colour is known, since the stroke colour defaults to it.
  void resolveTextStroke(const PropertyMap& props, HTMLInheritedStyle& out);

  // `parseBoxLayout` per-side margin longhand handler. Hoisted out of the original lambda so
  // the project's "no lambda" rule is honoured.
  void applyMarginLonghand(const PropertyMap& props, const char* propName, float& outPx);

  // Reports the platform's own spelling for `out`'s resolved font pair through the font-face-name
  // sink. Skipped for pairs the parent already produced, which keeps the platform lookup to one
  // call per distinct pair instead of one per element.
  void applyFontFaceNames(HTMLInheritedStyle& out, const HTMLInheritedStyle& parent);

  HTMLDiagnosticSink& _diagnostics;
  HTMLValueParser& _valueParser;
  FontAvailabilityThunk _fontAvailabilityThunk = nullptr;
  void* _fontAvailabilityUserData = nullptr;
  FontFaceNameThunk _fontFaceNameThunk = nullptr;
  void* _fontFaceNameUserData = nullptr;

  // CSS class selectors (key = class name without the dot). Pre-parsed at <style>-collection
  // time so per-element resolution can copy entries instead of re-running ParseStyleString
  // for every node referencing the same class.
  std::unordered_map<std::string, PropertyMap> _classRules = {};

  // CSS element selectors (key = lower-case tag name). Same shape as `_classRules`.
  std::unordered_map<std::string, PropertyMap> _elementRules = {};

  // Cached resolved style per DOM node. Each entry merges inline + class + element rules + tag
  // defaults.
  //
  // Lifetime: the raw `DOMNode*` keys are valid only for a single Parse cycle. The DOM is owned
  // by `HTMLParserContext` via `shared_ptr<XMLDOM>` and is not mutated mid-parse, so keys never
  // dangle within one cycle. If the cascade is ever reused across parses, this cache must be
  // cleared first; switching to `weak_ptr<DOMNode>` would express the constraint at the cost of
  // a hash-table indirection.
  std::unordered_map<const DOMNode*, PropertyMap> _resolvedCache = {};

  // `@keyframes` rules collected from `<style>` blocks, keyed by keyframes name. The last
  // definition of a given name wins, matching CSS cascade order.
  std::unordered_map<std::string, html::CssKeyframesRule> _keyframes = {};
};

}  // namespace pagx
