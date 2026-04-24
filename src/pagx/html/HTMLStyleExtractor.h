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
//  license is distributed on "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the details.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>

namespace pagx {

class HTMLStyleExtractor {
 public:
  /**
   * Consolidates every `style="..."` attribute in the input HTML into a
   * internal stylesheet injected immediately before `</head>`. Styles are
   * parsed into individual CSS properties, grouped by property-name signature,
   * and classified into shared (base) vs. varying (modifier) properties.
   *
   * When a group of 2+ elements shares the same property names and has ≥2
   * shared properties and ≤2 varying properties, a base class containing the
   * shared declarations is emitted alongside individual modifier classes for
   * the varying properties. Elements in such groups receive two class names:
   * `class="baseN modN"`.
   *
   * Styles that do not form viable groups fall back to exact-string
   * deduplication. All class names use semantic prefixes derived from the
   * element's tag name and existing class attribute (e.g., "blend", "layer",
   * "text", "bg", "root", "svg", "div") instead of the opaque "ps" prefix.
   *
   * The document's <body> tag style is deliberately left inline: its style is
   * unique per document (canvas size differs) so deduplication has no benefit.
   *
   * HTML entities in style values (e.g. `&#39;`) are decoded before parsing
   * because the CSS parser does not reinterpret named or numeric character
   * references inside a <style> element. Semicolons and colons inside
   * parentheses (e.g., in data: URIs or gradient functions) are respected
   * during property splitting.
   *
   * The input must be well-formed HTML produced by HTMLWriter. Behaviour on
   * malformed input is best-effort: the extractor will not crash but the
   * output is not guaranteed to be semantically identical.
   */
  static std::string Extract(const std::string& html);
};

}  // namespace pagx
