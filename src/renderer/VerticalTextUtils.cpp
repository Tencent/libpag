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

#include "VerticalTextUtils.h"
#include <cstddef>

namespace pagx {

// Upright ranges from Unicode VerticalOrientation-16.0.0.txt (UAX #50).
// Characters with orientation U or Tu (fallback Upright) are listed.
// Characters with orientation R or Tr (fallback Rotated) use the default.
// clang-format off
static const int32_t UprightRanges[][2] = {
    {0x000A7, 0x000A7},  // § Section Sign
    {0x000A9, 0x000A9},  // © Copyright Sign
    {0x000AE, 0x000AE},  // ® Registered Sign
    {0x000B1, 0x000B1},  // ± Plus-Minus Sign
    {0x000BC, 0x000BE},  // ¼..¾ Vulgar Fractions
    {0x000D7, 0x000D7},  // × Multiplication Sign
    {0x000F7, 0x000F7},  // ÷ Division Sign
    {0x002EA, 0x002EB},  // Modifier Letters (Yin/Yang Departing Tone)
    {0x01100, 0x011FF},  // Hangul Jamo
    {0x01401, 0x0167F},  // Canadian Syllabics
    {0x018B0, 0x018FF},  // Canadian Syllabics Extended
    {0x02016, 0x02016},  // ‖ Double Vertical Line
    {0x02020, 0x02021},  // † ‡ Dagger, Double Dagger
    {0x02030, 0x02031},  // ‰ ‱ Per Mille/Ten Thousand Sign
    {0x0203B, 0x0203C},  // ※ ‼ Reference Mark, Double Exclamation
    {0x02042, 0x02042},  // ⁂ Asterism
    {0x02047, 0x02049},  // ⁇ ⁈ ⁉ Double Question/Exclamation Marks
    {0x02051, 0x02051},  // ⁑ Two Asterisks Aligned Vertically
    {0x02065, 0x02065},  // Reserved Default_Ignorable_Code_Point
    {0x020DD, 0x020E0},  // Combining Enclosing Marks
    {0x020E2, 0x020E4},  // Combining Enclosing Marks
    {0x02100, 0x02101},  // ℀ ℁ Account Of, Addressed to the Subject
    {0x02103, 0x02109},  // ℃..℉ Degree Celsius..Fahrenheit
    {0x0210F, 0x0210F},  // ℏ Planck Constant
    {0x02113, 0x02114},  // ℓ Script Small L, L B Bar
    {0x02116, 0x02117},  // № ℗ Numero Sign, Sound Recording Copyright
    {0x0211E, 0x02123},  // ℞..℣ Prescription Take..Versicle
    {0x02125, 0x02125},  // ℥ Ounce Sign
    {0x02127, 0x02127},  // ℧ Inverted Ohm Sign
    {0x02129, 0x02129},  // ℩ Turned Greek Small Letter Iota
    {0x0212E, 0x0212E},  // ℮ Estimated Symbol
    {0x02135, 0x0213F},  // ℵ..ℿ Hebrew Letters..Double-Struck Pi
    {0x02145, 0x0214A},  // ⅅ..⅊ Double-Struck Italic..Property Line
    {0x0214C, 0x0214D},  // ⅌ ⅍ Per Sign, Aktieselskab
    {0x0214F, 0x02189},  // ⅏..↉ Samaritan Source Symbol..Fractions..Roman Numerals
    {0x0218C, 0x0218F},  // Reserved Number Forms
    {0x0221E, 0x0221E},  // ∞ Infinity
    {0x02234, 0x02235},  // ∴ ∵ Therefore, Because
    {0x02300, 0x02307},  // ⌀..⌇ Miscellaneous Technical
    {0x0230C, 0x0231F},  // ⌌..⌟ Crops and Corners
    {0x02324, 0x02328},  // ⌤..⌨ Miscellaneous Technical
    {0x0232B, 0x0232B},  // ⌫ Erase to the Left
    {0x0237D, 0x0239A},  // ⍽..⎚ Miscellaneous Technical
    {0x023BE, 0x023CD},  // ⎾..⏍ Dentistry Symbols
    {0x023CF, 0x023CF},  // ⏏ Eject Symbol
    {0x023D1, 0x023DB},  // ⏑..⏛ Metrical Symbols
    {0x023E2, 0x02422},  // ⏢..␢ Technical Symbols..Control Pictures
    {0x02424, 0x024FF},  // ␤..⓿ Control Pictures..Enclosed Alphanumerics
    {0x025A0, 0x02619},  // ■..☙ Geometric Shapes..Dingbats
    {0x02620, 0x02767},  // ☠..❧ Miscellaneous Symbols..Dingbats
    {0x02776, 0x02793},  // ❶..⓳ Dingbat Circled Digits
    {0x02B12, 0x02B2F},  // ⬒..⬯ Miscellaneous Symbols
    {0x02B50, 0x02B59},  // ⭐..⭙ Miscellaneous Symbols
    {0x02B97, 0x02B97},  // ⮗ Symbol for Type A Electronics
    {0x02BB8, 0x02BD1},  // ⮸..⯑ Arrows and Symbols
    {0x02BD3, 0x02BEB},  // ⯓..⯫ Astronomical Symbols
    {0x02BF0, 0x02BFF},  // ⯰..⯿ Astronomical Symbols
    {0x02E50, 0x02E51},  // ⹐ ⹑ Cross Patty Symbols
    {0x02E80, 0x0A4CF},  // CJK Radicals..Ideographic..Brackets..Kana..Katakana..Yi Radicals
    {0x0A960, 0x0A97F},  // Hangul Jamo Extended-A
    {0x0AC00, 0x0D7FF},  // Hangul Syllables..Jamo Extended-B
    {0x0E000, 0x0FAFF},  // PUA..CJK Compatibility Ideographs
    {0x0FE10, 0x0FE1F},  // Vertical Forms
    {0x0FE30, 0x0FE48},  // CJK Compatibility Forms (presentation forms for vertical)
    {0x0FE50, 0x0FE57},  // Small Form Variants (comma..exclamation)
    {0x0FE5F, 0x0FE62},  // Small Form Variants (number sign..plus)
    {0x0FE67, 0x0FE6F},  // Small Form Variants (reverse solidus..commercial at)
    {0x0FF01, 0x0FF07},  // ！..＇ Fullwidth Exclamation..Apostrophe
    {0x0FF0A, 0x0FF0C},  // ＊ ＋ ， Fullwidth Asterisk..Comma
    {0x0FF0E, 0x0FF19},  // ．..９ Fullwidth Period..Digit Nine
    {0x0FF1F, 0x0FF3A},  // ？..Ｚ Fullwidth Question Mark..Latin Capital Z
    {0x0FF3C, 0x0FF3C},  // ＼ Fullwidth Reverse Solidus
    {0x0FF3E, 0x0FF3E},  // ＾ Fullwidth Circumflex
    {0x0FF40, 0x0FF5A},  // ｀..ｚ Fullwidth Grave..Latin Small Z
    {0x0FFE0, 0x0FFE2},  // ￠ ￡ ￢ Fullwidth Cent..Not Sign
    {0x0FFE4, 0x0FFE7},  // ￤..￧ Fullwidth Broken Bar..Yen/Won..Reserved
    {0x0FFF0, 0x0FFF8},  // Reserved Specials
    {0x0FFFC, 0x0FFFD},  // ￼ � Object Replacement..Replacement Character
    {0x10980, 0x1099F},  // Meroitic Hieroglyphs
    {0x11580, 0x115FF},  // Siddham
    {0x11A00, 0x11ABF},  // Zanabazar Square..Soyombo..Canadian Syllabics
    {0x13000, 0x1467F},  // Egyptian Hieroglyphs..Anatolian Hieroglyphs
    {0x16FE0, 0x18D7F},  // Ideographic Symbols..Tangut..Khitan Small Script
    {0x1AFF0, 0x1B2FF},  // Kana Extended-B..Small Kana Extension..Nushu
    {0x1CF00, 0x1CFCF},  // Znamenny Musical Notation
    {0x1D000, 0x1D1FF},  // Byzantine..Western Musical Symbols
    {0x1D2E0, 0x1D37F},  // Mayan Numerals..Counting Rod..Tai Xuan Jing
    {0x1D800, 0x1DAAF},  // Sutton SignWriting
    {0x1F000, 0x1F7FF},  // Mahjong..Domino..Playing Cards..Enclosed..Emoji..Geometric Ext
    {0x1F900, 0x1FAFF},  // Supplemental Symbols..Chess Symbols..Pictographs Ext-A
    {0x20000, 0x2FFFD},  // CJK Unified Ideographs Extension B..Compatibility Supplement
    {0x30000, 0x3FFFD},  // CJK Unified Ideographs Extension G..I
    {0xF0000, 0xFFFFD},  // Supplementary PUA-A
    {0x100000, 0x10FFFD},  // Supplementary PUA-B
};
// clang-format on

static constexpr size_t UprightRangeCount = sizeof(UprightRanges) / sizeof(UprightRanges[0]);

VerticalOrientation VerticalTextUtils::GetOrientation(int32_t c) {
  // Binary search over sorted upright ranges from UAX #50 VerticalOrientation-16.0.0.txt.
  // Characters with orientation U (Upright) or Tu (fallback Upright) return Upright.
  // Characters with orientation R (Rotated) or Tr (fallback Rotated) return Rotated.
  size_t lo = 0;
  size_t hi = UprightRangeCount;
  while (lo < hi) {
    auto mid = lo + (hi - lo) / 2;
    if (c > UprightRanges[mid][1]) {
      lo = mid + 1;
    } else if (c < UprightRanges[mid][0]) {
      hi = mid;
    } else {
      return VerticalOrientation::Upright;
    }
  }
  return VerticalOrientation::Rotated;
}

bool VerticalTextUtils::NeedsPunctuationOffset(int32_t c) {
  // Tu (Transformed-Upright) punctuation per UTR#50 that occupies the lower-left area in
  // horizontal layout but should appear in the upper-right area in vertical layout.
  // Only includes characters with vo=Tu that need positional adjustment.
  // Note: FF1A (fullwidth colon) and FF1B (fullwidth semicolon) are vo=Tr (Rotated), not Tu.
  return c == 0x3001 ||  // 、 ideographic comma
         c == 0x3002 ||  // 。 ideographic full stop
         c == 0xFF0C ||  // ， fullwidth comma
         c == 0xFF0E;    // ． fullwidth full stop
}

void VerticalTextUtils::GetPunctuationOffset(float fontSize, float* outDx, float* outDy) {
  if (outDx) {
    *outDx = fontSize * 0.5f;
  }
  if (outDy) {
    *outDy = -fontSize * 0.5f;
  }
}

}  // namespace pagx
