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
 * Simplified UAX#14 Line Break Class. The full 43 classes are merged into 16 based on identical
 * pair-table behavior, keeping binary size minimal while covering CJK kinsoku, emoji, numeric
 * affixes, quotation marks, and zero-width break controls.
 */
enum class LineBreakClass : uint8_t {
  OP,  // Opening punctuation (UAX#14 OP)
  CL,  // Closing punctuation (UAX#14 CL, CP)
  QU,  // Quotation marks (UAX#14 QU)
  NS,  // Non-starters / kinsoku line-start prohibited (UAX#14 NS, CJ)
  EX,  // Exclamation / question (UAX#14 EX)
  GL,  // Glue / non-breaking (UAX#14 GL, WJ)
  BA,  // Break after (UAX#14 BA, HY, SY, ZW)
  BB,  // Break before (UAX#14 BB)
  IN,  // Inseparable (UAX#14 IN)
  PR,  // Prefix numeric (UAX#14 PR)
  PO,  // Postfix numeric (UAX#14 PO)
  NU,  // Numeric (UAX#14 NU)
  AL,  // Alphabetic — default class (UAX#14 AL, HL, SA, AI, XX)
  ID,  // Ideographic / emoji base (UAX#14 ID, EB, H2, H3)
  CM,  // Combining mark / emoji component (UAX#14 CM, ZWJ, EM, RI)
  SP,  // Space (UAX#14 SP)
};

/**
 * LineBreaker provides Unicode-aware line breaking based on a simplified UAX#14 pair table.
 * It covers CJK ideographs, kinsoku shori, emoji sequences, numeric affixes, quotation binding,
 * zero-width break controls (ZWSP, Word Joiner), and soft hyphens. This implementation does not
 * depend on ICU and uses a compact range table (~1.3KB total) for minimal binary size.
 */
class LineBreaker {
 public:
  /**
   * Returns true if a line break is allowed between the two Unicode code points. The decision is
   * based on the UAX#14 pair table lookup of their line break classes, with special handling for
   * combining marks and space-mediated indirect breaks.
   * @param prevChar The Unicode code point before the potential break.
   * @param nextChar The Unicode code point after the potential break.
   */
  static bool canBreakBetween(int32_t prevChar, int32_t nextChar);

  /**
   * Returns the simplified UAX#14 line break class for a Unicode code point. Uses a sorted range
   * table with binary search. Characters not in the table default to AL (Alphabetic).
   */
  static LineBreakClass GetLineBreakClass(int32_t unichar);

  /**
   * Returns true if the character is a CJK ideograph or syllable (Han, Hiragana, Katakana,
   * Hangul, etc.). This is a broader check than ID class — it includes characters in the CJK
   * Symbols and Punctuation block that may have OP/CL/NS classes.
   */
  static bool isCJK(int32_t unichar);

  /**
   * Returns true if the character is a whitespace character (space, tab, etc.).
   */
  static bool isWhitespace(int32_t unichar);
};

}  // namespace pagx
