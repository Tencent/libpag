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
 * Tokenises CSS text into top-level `(selectors, declarations)` rules. Most `@`-rules (`@media`,
 * `@font-face`, ...) are skipped: the matching brace is tracked so parsing stays synchronised,
 * but their body is not emitted (a single record describing the dropped at-rule is appended to
 * `droppedAtRules` so callers can surface diagnostics).
 *
 * `@keyframes` / `@-webkit-keyframes` rules are parsed into `keyframesRules` instead of being
 * dropped, so the HTML animation subset (see `spec/html_subset.md` §13) can map them onto PAGX
 * animations. Pass the no-keyframes overload when keyframes are not relevant.
 *
 * Block comments are stripped. The tokeniser is intentionally permissive: malformed
 * input causes the offending block to be skipped rather than raising an error.
 */
std::vector<RawCssRule> TokenizeStyleSheet(const std::string& css,
                                           std::vector<std::string>& droppedAtRules,
                                           std::vector<CssKeyframesRule>& keyframesRules);

/**
 * Convenience overload that discards any `@keyframes` rules (forwards to the three-argument form
 * with a throwaway sink). Kept so call sites that do not care about animations stay terse.
 */
std::vector<RawCssRule> TokenizeStyleSheet(const std::string& css,
                                           std::vector<std::string>& droppedAtRules);

/**
 * Serialises a list of `@keyframes` rules back into canonical CSS text (one rule per line). Used
 * by the subset transformer to re-emit a `<style>` block that carries only the surviving
 * keyframes so the importer can re-read them after the cascade has been inlined.
 */
std::string SerializeKeyframes(const std::vector<CssKeyframesRule>& keyframesRules);

}  // namespace pagx::html
