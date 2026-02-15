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
 * Vertical orientation of a character in vertical text layout, based on UTR#50.
 */
enum class VerticalOrientation {
  /**
   * Character is displayed upright (not rotated). Used for CJK characters.
   */
  Upright,
  /**
   * Character is rotated 90° clockwise. Used for Latin letters, digits, and most
   * Western punctuation.
   */
  Rotated
};

/**
 * Transform type for CJK punctuation in vertical text layout.
 */
enum class PunctuationTransform {
  /**
   * No transform needed — character displays correctly as-is in vertical layout.
   */
  None,
  /**
   * Rotate the character 90° clockwise (e.g., brackets, dashes, ellipsis).
   */
  Rotate90,
  /**
   * Offset the character position to the upper-right area of the character cell
   * (e.g., comma, period, colon in CJK vertical text).
   */
  Offset
};

/**
 * VerticalTextUtils provides utilities for vertical text layout, including UTR#50 character
 * orientation classification and CJK punctuation transform rules. This implements the same
 * approach as Adobe After Effects: manual transforms on horizontal glyphs rather than relying
 * on OpenType 'vert' feature.
 */
class VerticalTextUtils {
 public:
  /**
   * Returns the vertical orientation of a Unicode character based on UTR#50 classification.
   * CJK characters are Upright; Latin letters, digits, and Western punctuation are Rotated.
   */
  static VerticalOrientation getOrientation(int32_t unichar);

  /**
   * Returns the transform type needed for a CJK punctuation character in vertical text.
   * Returns None for non-punctuation characters.
   */
  static PunctuationTransform getPunctuationTransform(int32_t unichar);

  /**
   * Returns the position offset for punctuation characters that need the Offset transform.
   * The offset moves the glyph from its horizontal position (lower-left area) to the
   * vertical position (upper-right area).
   * @param unichar The Unicode code point of the punctuation character.
   * @param fontSize The font size used to calculate the offset magnitude.
   * @param outDx Output: horizontal offset (positive = rightward).
   * @param outDy Output: vertical offset (negative = upward).
   */
  static void getPunctuationOffset(int32_t unichar, float fontSize, float* outDx, float* outDy);

  /**
   * Returns true if the character is a Latin letter (basic + extended).
   */
  static bool isLatinLetter(int32_t unichar);

  /**
   * Returns true if the character is a digit (ASCII or fullwidth).
   */
  static bool isDigit(int32_t unichar);

  /**
   * Returns true if the character should be grouped with adjacent Latin/digit characters
   * for rotated-as-a-unit treatment in vertical text (the "tcy group" behavior).
   * This includes Latin letters, digits, and connecting punctuation like period and hyphen
   * within numbers/abbreviations.
   */
  static bool isRotatedGroupChar(int32_t unichar);
};

}  // namespace pagx
