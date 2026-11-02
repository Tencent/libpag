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

#include <string>
#include <vector>
#include "pagx/utils/CSSFontStyle.h"

namespace pagx::html {

enum class SelectorKind {
  Class,        // `.foo`
  Element,      // `body`, `h1`, etc.
  Universal,    // `*`
  Unsupported,  // descendant `.a .b`, pseudo, attribute, `@`, etc.
};

struct ParsedSelector {
  SelectorKind kind = SelectorKind::Unsupported;
  std::string key = {};  // class name (no dot) or tag name (lower-case), empty for unsupported
  std::string raw = {};  // original text for diagnostics
};

/**
 * Classify a single CSS selector token. The grammar accepted by the subset is intentionally
 * narrow: a bare class selector, a bare tag selector, or `*`. Anything else (combinators,
 * pseudo-classes, attribute selectors, `@`-rules) maps to `Unsupported`.
 *
 * The selector is expected to already be whitespace-trimmed.
 */
ParsedSelector ClassifySelector(const std::string& selector);

/**
 * A single `(selectors, declarations)` pair extracted from a `<style>` block. `selectors` is the
 * comma-separated list verbatim (already trimmed); `declarations` is the body between the
 * braces (semicolon-separated property:value pairs).
 */
struct RawCssRule {
  std::string selectors = {};
  std::string declarations = {};
};

/**
 * A single stop inside a `@keyframes` block. `percent` is the resolved keyframe offset in the
 * range [0, 100] (`from` resolves to 0, `to` to 100). `declarations` is the verbatim
 * semicolon-separated property:value body. A stop selector that lists several comma-separated
 * offsets is expanded into one `CssKeyframeStop` per offset, all sharing the same declarations.
 */
struct CssKeyframeStop {
  float percent = 0.0f;
  std::string declarations = {};
};

/**
 * A parsed `@keyframes <name> { … }` rule. `name` is the keyframes identifier (verbatim, used to
 * match the `animation-name` reference). `stops` are ordered by source order; callers sort by
 * `percent` when building keyframe sequences. `-webkit-keyframes` is accepted as an alias.
 */
struct CssKeyframesRule {
  std::string name = {};
  std::vector<CssKeyframeStop> stops = {};
};

/**
 * A parsed `@font-face { … }` rule: the family it declares, its `url()` sources in declaration
 * order, and the `font-weight` / `font-style` descriptors that say which face of the family the
 * rule declares. `local()` sources are ignored.
 *
 * `weight` and `style` keep the authored descriptor text verbatim (`700`, `400 900`, `italic`)
 * so the subset transformer can re-emit the rule unchanged, and stay empty when the descriptor is
 * missing or invalid — the face then keeps the CSS initial value `normal`.
 */
struct CssFontFaceRule {
  std::string family = {};
  std::vector<std::string> sources = {};
  std::string weight = {};
  std::string style = {};
};

/** The weight interval a `@font-face` `font-weight` descriptor declares; a static face declares a
 *  single weight, a variable face the range of its weight axis. Endpoints keep the precision of the
 *  descriptor, so a fractional range is validated and compared as authored. */
struct CssFontWeightRange {
  float minWeight = 400;
  float maxWeight = 400;

  bool contains(float weight) const {
    return weight >= minWeight && weight <= maxWeight;
  }
};

/** Parses a `font-weight` descriptor (`normal`, `bold`, `700` or a range `400 900`) into the
 *  interval it declares; an empty value reports the initial `{400, 400}`. Weights are CSS numbers in
 *  `[1, 1000]` and may be fractional. Returns false for a value that is not a valid descriptor. */
bool ParseFontFaceWeight(const std::string& value, CssFontWeightRange& out);

/** Parses a `font-style` descriptor (`normal`, `italic`, `oblique`) into `outItalic`; an empty
 *  value reports `normal`. Returns false for a value that is not a valid descriptor. */
bool ParseFontFaceStyle(const std::string& value, bool& outItalic);

/** The face a CSS request resolves to, with the weight and slant it actually delivers. */
struct CssFontFaceMatch {
  const CssFontFaceRule* rule = nullptr;
  // The request projected onto `rule`: the weight clamped into its declared range (keeping the
  // precision the descriptor declares) and the slant the rule actually provides.
  CSSFontStyle style = {};
};

/** Resolves the `@font-face` face of `family` that `request` renders with, the way a browser's
 *  font matching does: width, then slant, then the CSS weight preference order. Each rule is
 *  projected onto the request — its declared weight interval clamps the requested weight — so the
 *  shared face matcher ranks declared faces exactly as it ranks registered ones, and
 *  `out.style.weight` is the weight the face actually delivers: a family that only ships a 700
 *  face answers a 400 request with 700. Returns false when no rule declares `family`. */
bool SelectFontFace(const std::vector<CssFontFaceRule>& faces, const std::string& family,
                    const CSSFontStyle& request, CssFontFaceMatch& out);

/**
 * Tokenises CSS text into top-level `(selectors, declarations)` rules. Most `@`-rules (`@media`,
 * `@import`, ...) are skipped: the matching brace is tracked so parsing stays synchronised, but
 * their body is not emitted (a single record describing the dropped at-rule is appended to
 * `droppedAtRules` so callers can surface diagnostics).
 *
 * `@keyframes` / `@-webkit-keyframes` rules are parsed into `keyframesRules` instead of being
 * dropped, so the HTML animation subset (see `spec/html_subset.md` §13) can map them onto PAGX
 * animations. `@font-face` rules that declare a family and at least one `url()` source are parsed
 * into `fontFaceRules` so their font files can become PAGX Font resources; any other
 * `@font-face` (e.g. `local()`-only) is dropped like an unsupported at-rule.
 *
 * Block comments are stripped. The tokeniser is intentionally permissive: malformed
 * input causes the offending block to be skipped rather than raising an error.
 */
std::vector<RawCssRule> TokenizeStyleSheet(const std::string& css,
                                           std::vector<std::string>& droppedAtRules,
                                           std::vector<CssKeyframesRule>& keyframesRules,
                                           std::vector<CssFontFaceRule>& fontFaceRules);

/**
 * Serialises a list of `@keyframes` rules back into canonical CSS text (one rule per line). Used
 * by the subset transformer to re-emit a `<style>` block that carries only the surviving
 * keyframes so the importer can re-read them after the cascade has been inlined.
 */
std::string SerializeKeyframes(const std::vector<CssKeyframesRule>& keyframesRules);

/**
 * Serialises a list of `@font-face` rules back into CSS text (one rule per line) that
 * TokenizeStyleSheet parses into the same rules. Used by the subset transformer to carry the
 * font faces past the cascade-inlining pass, the same way it carries `@keyframes`. The
 * `font-weight` / `font-style` descriptors are re-emitted when the rule declares them, since the
 * cascade reads them to pick the face a text request resolves to.
 */
std::string SerializeFontFaces(const std::vector<CssFontFaceRule>& fontFaceRules);

}  // namespace pagx::html
