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

#include <cstdint>

namespace pagx {

/**
 * LineBreaker provides Unicode-aware line breaking utilities for text layout.
 * It supports CJK character detection, word boundary detection, and CJK line break prohibition
 * rules (kinsoku shori). This implementation does not depend on ICU and uses hard-coded Unicode
 * ranges for minimal binary size.
 */
class LineBreaker {
 public:
  /**
   * Returns true if a line break is allowed between the two Unicode code points.
   * This considers whitespace boundaries, CJK character boundaries, hyphen breaks,
   * and CJK prohibition rules (kinsoku shori).
   * @param prevChar The Unicode code point before the potential break.
   * @param nextChar The Unicode code point after the potential break.
   */
  static bool canBreakBetween(int32_t prevChar, int32_t nextChar);

  /**
   * Returns true if the character is a CJK ideograph or syllable that allows breaking
   * before and after it (unless prohibited by kinsoku rules).
   */
  static bool isCJK(int32_t unichar);

  /**
   * Returns true if the character is a whitespace character (space, tab, etc.).
   */
  static bool isWhitespace(int32_t unichar);

  /**
   * Returns true if the character must not appear at the start of a line (kinsoku).
   * This includes closing brackets, punctuation marks like period, comma, etc.
   */
  static bool isLineStartProhibited(int32_t unichar);

  /**
   * Returns true if the character must not appear at the end of a line (kinsoku).
   * This includes opening brackets and similar characters.
   */
  static bool isLineEndProhibited(int32_t unichar);

  /**
   * Returns true if a line break is allowed after this character.
   * This is true for hyphens and similar break-after characters.
   */
  static bool isBreakAfter(int32_t unichar);

  /**
   * Returns true if the character is an emoji component that should not be separated from its
   * base character (ZWJ, variation selectors, skin tone modifiers, regional indicators, etc.).
   */
  static bool isEmojiComponent(int32_t unichar);

  /**
   * Returns true if the character is a Unicode soft hyphen (U+00AD).
   */
  static bool isSoftHyphen(int32_t unichar);
};

}  // namespace pagx
