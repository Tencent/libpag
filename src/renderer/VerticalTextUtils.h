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
  static VerticalOrientation GetOrientation(int32_t unichar);

  /**
   * Returns true if the character needs position offset in vertical text layout. Characters like
   * fullwidth comma, period, and colon occupy the lower-left area in horizontal layout but should
   * appear in the upper-right area in vertical layout.
   */
  static bool NeedsPunctuationOffset(int32_t unichar);

  /**
   * Returns the position offset for punctuation characters in vertical text layout.
   * The offset moves the glyph from its horizontal position (lower-left area) to the
   * vertical position (upper-right area).
   * @param fontSize The font size used to calculate the offset magnitude.
   * @param outDx Output: horizontal offset (positive = rightward).
   * @param outDy Output: vertical offset (negative = upward).
   */
  static void GetPunctuationOffset(float fontSize, float* outDx, float* outDy);
};

}  // namespace pagx
