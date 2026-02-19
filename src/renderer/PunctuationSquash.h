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
 * CJK fullwidth punctuation categories, aligned with JLREQ character classes and CSS Text Level 4
 * text-spacing-trim definitions. Each fullwidth CJK punctuation occupies one em but has internal
 * whitespace on one or both sides that can be removed (squashed).
 *
 * - Opening: whitespace on the leading side (before the glyph face). JLREQ cl-01.
 * - Closing: whitespace on the trailing side (after the glyph face). JLREQ cl-02, cl-04, cl-06,
 *   cl-07.
 * - MiddleDot: whitespace on both sides (centered in the em). JLREQ cl-05.
 */
enum class PunctuationCategory {
  None,
  Opening,
  Closing,
  MiddleDot
};

/**
 * Result of computing the squash adjustment between two adjacent punctuation characters.
 * Each field represents the fraction of that character's advance to remove (0.0 = no squash,
 * 0.5 = remove half the advance).
 */
struct SquashResult {
  float prevSquash = 0;
  float nextSquash = 0;
};

class PunctuationSquash {
 public:
  /**
   * Returns the punctuation category of a CJK fullwidth punctuation character. Returns
   * PunctuationCategory::None for non-CJK or non-fullwidth characters. The classification follows
   * JLREQ character classes: cl-01 maps to Opening, cl-02/cl-04/cl-06/cl-07 map to Closing, and
   * cl-05 maps to MiddleDot.
   * @param unichar A Unicode code point.
   */
  static PunctuationCategory GetCategory(int32_t unichar);

  /**
   * Returns the squash fractions for two adjacent characters based on JLREQ Table 1 spacing rules.
   * The returned prevSquash/nextSquash values represent the fraction of each character's advance to
   * remove. Returns zero fractions if no squashing applies.
   */
  static SquashResult GetAdjacentSquash(int32_t prevChar, int32_t nextChar);

  /**
   * Returns the squash fraction for a punctuation character at the start of a line (or column in
   * vertical layout). Only Opening punctuation is squashed at line start per JLREQ/CLREQ rules.
   */
  static float GetLineStartSquash(int32_t unichar);

  /**
   * Returns the squash fraction for a punctuation character at the end of a line (or column in
   * vertical layout). Only Closing punctuation is squashed at line end per JLREQ/CLREQ rules.
   */
  static float GetLineEndSquash(int32_t unichar);
};

}  // namespace pagx
