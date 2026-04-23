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
   * Consolidates every `style="..."` attribute in the input HTML into a single
   * internal stylesheet injected immediately before `</head>`. Each unique
   * style value becomes a class rule named `.psN` (N starting at 0, in
   * first-seen order). Existing `class="..."` attributes are preserved and the
   * generated class name is appended after a space.
   *
   * The document's <body> tag style is deliberately left inline: its style is
   * unique per document (canvas size differs) so deduplication has no benefit.
   *
   * Style values are treated as opaque strings — no CSS parsing. HTML entities
   * in the value (e.g. `&#39;`) are decoded before being written into the
   * <style> block because the CSS parser does not reinterpret named or numeric
   * character references inside a <style> element.
   *
   * The input must be well-formed HTML produced by HTMLWriter. Behaviour on
   * malformed input is best-effort: the extractor will not crash but the
   * output is not guaranteed to be semantically identical.
   */
  static std::string Extract(const std::string& html);
};

}  // namespace pagx
