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
#include "pagx/html/HTMLTransformContext.h"

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
 * Tokenises CSS text into top-level `(selectors, declarations)` rules. `@`-rules (`@media`,
 * `@font-face`, `@keyframes`, ...) are skipped: the matching brace is tracked so parsing stays
 * synchronised, but their body is not emitted (a single record describing the dropped at-rule
 * is appended to `droppedAtRules` so callers can surface diagnostics).
 *
 * Block comments are stripped. The tokeniser is intentionally permissive: malformed
 * input causes the offending block to be skipped rather than raising an error.
 */
std::vector<RawCssRule> TokenizeStyleSheet(const std::string& css,
                                           std::vector<std::string>& droppedAtRules);

}  // namespace pagx::html
