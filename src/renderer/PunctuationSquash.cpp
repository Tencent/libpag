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

#include "PunctuationSquash.h"

namespace pagx {

// Half of a fullwidth character advance. CJK fullwidth punctuation glyphs typically occupy half
// the em-square, with the other half being whitespace.
static constexpr float HALF = 0.5f;

// Quarter of a fullwidth character advance. Used for middle dot punctuation which has quarter-em
// whitespace on each side.
static constexpr float QUARTER = 0.25f;

PunctuationCategory PunctuationSquash::GetCategory(int32_t c) {
  switch (c) {
    // Opening brackets (JLREQ cl-01): whitespace on the leading (left in LTR, top in vertical)
    // side.
    case 0xFF08:  // （ FULLWIDTH LEFT PARENTHESIS
    case 0x3008:  // 〈 LEFT ANGLE BRACKET
    case 0x300A:  // 《 LEFT DOUBLE ANGLE BRACKET
    case 0x300C:  // 「 LEFT CORNER BRACKET
    case 0x300E:  // 『 LEFT WHITE CORNER BRACKET
    case 0x3010:  // 【 LEFT BLACK LENTICULAR BRACKET
    case 0x3014:  // 〔 LEFT TORTOISE SHELL BRACKET
    case 0x3016:  // 〖 LEFT WHITE LENTICULAR BRACKET
    case 0x3018:  // 〘 LEFT WHITE TORTOISE SHELL BRACKET
    case 0x301A:  // 〚 LEFT WHITE SQUARE BRACKET
    case 0xFF3B:  // ［ FULLWIDTH LEFT SQUARE BRACKET
    case 0xFF5B:  // ｛ FULLWIDTH LEFT CURLY BRACKET
    case 0x2018:  // ' LEFT SINGLE QUOTATION MARK
    case 0x201C:  // " LEFT DOUBLE QUOTATION MARK
      return PunctuationCategory::Opening;

    // Closing brackets, commas, periods, and dividing punctuation (JLREQ cl-02, cl-04, cl-06,
    // cl-07): whitespace on the trailing (right in LTR, bottom in vertical) side.
    case 0xFF09:  // ） FULLWIDTH RIGHT PARENTHESIS
    case 0x3009:  // 〉 RIGHT ANGLE BRACKET
    case 0x300B:  // 》 RIGHT DOUBLE ANGLE BRACKET
    case 0x300D:  // 」 RIGHT CORNER BRACKET
    case 0x300F:  // 』 RIGHT WHITE CORNER BRACKET
    case 0x3011:  // 】 RIGHT BLACK LENTICULAR BRACKET
    case 0x3015:  // 〕 RIGHT TORTOISE SHELL BRACKET
    case 0x3017:  // 〗 RIGHT WHITE LENTICULAR BRACKET
    case 0x3019:  // 〙 RIGHT WHITE TORTOISE SHELL BRACKET
    case 0x301B:  // 〛 RIGHT WHITE SQUARE BRACKET
    case 0xFF3D:  // ］ FULLWIDTH RIGHT SQUARE BRACKET
    case 0xFF5D:  // ｝ FULLWIDTH RIGHT CURLY BRACKET
    case 0x2019:  // ' RIGHT SINGLE QUOTATION MARK
    case 0x201D:  // " RIGHT DOUBLE QUOTATION MARK
    case 0x3001:  // 、 IDEOGRAPHIC COMMA
    case 0x3002:  // 。 IDEOGRAPHIC FULL STOP
    case 0xFF0C:  // ， FULLWIDTH COMMA
    case 0xFF0E:  // ． FULLWIDTH FULL STOP
    case 0xFF01:  // ！ FULLWIDTH EXCLAMATION MARK (JLREQ cl-04)
    case 0xFF1F:  // ？ FULLWIDTH QUESTION MARK (JLREQ cl-04)
    case 0xFF1A:  // ： FULLWIDTH COLON
    case 0xFF1B:  // ； FULLWIDTH SEMICOLON
      return PunctuationCategory::Closing;

    // Middle dot punctuation (JLREQ cl-05): whitespace on both sides (quarter-em each side).
    case 0x30FB:  // ・ KATAKANA MIDDLE DOT
    case 0xFF65:  // ･ HALFWIDTH KATAKANA MIDDLE DOT
      return PunctuationCategory::MiddleDot;

    default:
      return PunctuationCategory::None;
  }
}

// JLREQ Table 1 inter-character spacing rules for adjacent fullwidth punctuation:
//
// When two fullwidth punctuation characters are adjacent, the whitespace between them should be
// removed or reduced. Each fullwidth punctuation has internal whitespace:
//   - Opening (cl-01): half-em whitespace before the glyph face
//   - Closing (cl-02/04/06/07): half-em whitespace after the glyph face
//   - MiddleDot (cl-05): quarter-em whitespace on each side
//
// The squash removes the internal whitespace that faces the adjacent character:
//
//   prev \ next  | Opening      | Closing      | MiddleDot
//   -------------|--------------|--------------|-------------
//   Opening      | prev: HALF   | (no squash)  | (no squash)
//   Closing      | prev+next: H | next: HALF   | next: QUARTER
//   MiddleDot    | prev+next:Q+H| prev: QUARTER| prev+next: Q
//
// Notes:
// - Closing + Opening: both sides squashed (trailing space of prev + leading space of next)
// - MiddleDot + Opening: both sides squashed (trailing quarter of prev + leading half of next)
// - Opening + Opening: only prev squashed (trailing space removed)
// - Closing + Closing: only next squashed (leading space removed)
// - Opening + Closing / Opening + MiddleDot: no squash (the glyph faces are already adjacent)

SquashResult PunctuationSquash::GetAdjacentSquash(int32_t prevChar, int32_t nextChar) {
  auto prevCat = GetCategory(prevChar);
  auto nextCat = GetCategory(nextChar);

  if (prevCat == PunctuationCategory::None || nextCat == PunctuationCategory::None) {
    return {0, 0};
  }

  switch (prevCat) {
    case PunctuationCategory::Opening:
      switch (nextCat) {
        case PunctuationCategory::Opening:
          // 「「: remove trailing space of prev opening bracket.
          return {HALF, 0};
        default:
          // Opening + Closing or Opening + MiddleDot: glyph faces already adjacent, no squash.
          return {0, 0};
      }

    case PunctuationCategory::Closing:
      switch (nextCat) {
        case PunctuationCategory::Opening:
          // 。「: remove trailing space of prev + leading space of next.
          return {HALF, HALF};
        case PunctuationCategory::Closing:
          // 。」: remove leading space of next closing bracket.
          return {0, HALF};
        case PunctuationCategory::MiddleDot:
          // 。・: remove trailing quarter of next middle dot.
          return {0, QUARTER};
        default:
          return {0, 0};
      }

    case PunctuationCategory::MiddleDot:
      switch (nextCat) {
        case PunctuationCategory::Opening:
          // ・「: remove trailing quarter of prev + leading half of next.
          return {QUARTER, HALF};
        case PunctuationCategory::Closing:
          // ・。: remove trailing quarter of prev.
          return {QUARTER, 0};
        case PunctuationCategory::MiddleDot:
          // ・・: remove trailing quarter of prev + leading quarter of next.
          return {QUARTER, QUARTER};
        default:
          return {0, 0};
      }

    default:
      return {0, 0};
  }
}

float PunctuationSquash::GetLineStartSquash(int32_t unichar) {
  auto cat = GetCategory(unichar);
  if (cat == PunctuationCategory::Opening) {
    return HALF;
  }
  return 0;
}

float PunctuationSquash::GetLineEndSquash(int32_t unichar) {
  auto cat = GetCategory(unichar);
  if (cat == PunctuationCategory::Closing) {
    return HALF;
  }
  return 0;
}

}  // namespace pagx
