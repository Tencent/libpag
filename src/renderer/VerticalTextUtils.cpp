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

namespace pagx {

bool VerticalTextUtils::isLatinLetter(int32_t c) {
  // Basic Latin letters
  if (c >= 0x0041 && c <= 0x005A) return true;  // A-Z
  if (c >= 0x0061 && c <= 0x007A) return true;  // a-z
  // Latin Extended-A
  if (c >= 0x00C0 && c <= 0x024F) return true;
  // Latin Extended-B
  if (c >= 0x0250 && c <= 0x02AF) return true;
  // Latin Extended Additional
  if (c >= 0x1E00 && c <= 0x1EFF) return true;
  return false;
}

bool VerticalTextUtils::isDigit(int32_t c) {
  // ASCII digits
  if (c >= 0x0030 && c <= 0x0039) return true;
  // Fullwidth digits
  if (c >= 0xFF10 && c <= 0xFF19) return true;
  return false;
}

bool VerticalTextUtils::isRotatedGroupChar(int32_t c) {
  // Characters that form a "rotation group" in vertical text:
  // Latin letters, digits, and connecting punctuation within words/numbers
  if (isLatinLetter(c)) return true;
  if (isDigit(c)) return true;

  // Connecting punctuation that stays with Latin/digit groups
  switch (c) {
    case '.':     // period (in abbreviations like "U.S.A.")
    case ',':     // comma (in numbers like "1,000")
    case '-':     // hyphen
    case '\'':    // apostrophe
    case '/':     // slash (in dates, fractions)
    case ':':     // colon (in times "12:00")
    case '%':     // percent
    case '#':     // hash
    case '@':     // at sign
    case '&':     // ampersand
    case '+':     // plus
    case '=':     // equals
    case '$':     // dollar
    case 0x20AC:  // €  euro
    case 0x00A3:  // £  pound
    case 0x00A5:  // ¥  yen
      return true;
    default:
      return false;
  }
}

VerticalOrientation VerticalTextUtils::getOrientation(int32_t c) {
  // CJK Unified Ideographs and Extensions
  if (c >= 0x4E00 && c <= 0x9FFF) return VerticalOrientation::Upright;
  if (c >= 0x3400 && c <= 0x4DBF) return VerticalOrientation::Upright;
  if (c >= 0x20000 && c <= 0x2A6DF) return VerticalOrientation::Upright;
  if (c >= 0x2A700 && c <= 0x2B73F) return VerticalOrientation::Upright;
  if (c >= 0x2B740 && c <= 0x2B81F) return VerticalOrientation::Upright;
  if (c >= 0xF900 && c <= 0xFAFF) return VerticalOrientation::Upright;
  if (c >= 0x2F800 && c <= 0x2FA1F) return VerticalOrientation::Upright;

  // Hiragana
  if (c >= 0x3040 && c <= 0x309F) return VerticalOrientation::Upright;
  // Katakana
  if (c >= 0x30A0 && c <= 0x30FF) return VerticalOrientation::Upright;
  // Katakana Phonetic Extensions
  if (c >= 0x31F0 && c <= 0x31FF) return VerticalOrientation::Upright;

  // Hangul
  if (c >= 0xAC00 && c <= 0xD7AF) return VerticalOrientation::Upright;
  if (c >= 0x1100 && c <= 0x11FF) return VerticalOrientation::Upright;
  if (c >= 0x3130 && c <= 0x318F) return VerticalOrientation::Upright;

  // Bopomofo
  if (c >= 0x3100 && c <= 0x312F) return VerticalOrientation::Upright;
  if (c >= 0x31A0 && c <= 0x31BF) return VerticalOrientation::Upright;

  // CJK Radicals
  if (c >= 0x2E80 && c <= 0x2EFF) return VerticalOrientation::Upright;
  if (c >= 0x2F00 && c <= 0x2FDF) return VerticalOrientation::Upright;

  // CJK Symbols and Punctuation (some are upright in vertical text)
  // These are handled by getPunctuationTransform for specific behavior
  if (c >= 0x3000 && c <= 0x303F) return VerticalOrientation::Upright;

  // Enclosed CJK Letters
  if (c >= 0x3200 && c <= 0x32FF) return VerticalOrientation::Upright;
  // CJK Compatibility
  if (c >= 0x3300 && c <= 0x33FF) return VerticalOrientation::Upright;

  // Fullwidth forms — CJK punctuation marks are upright
  if (c >= 0xFF01 && c <= 0xFF60) return VerticalOrientation::Upright;

  // Yi Syllables and Radicals
  if (c >= 0xA000 && c <= 0xA4CF) return VerticalOrientation::Upright;

  // Ideographic Description Characters
  if (c >= 0x2FF0 && c <= 0x2FFF) return VerticalOrientation::Upright;

  // Everything else (Latin, Cyrillic, Greek, digits, Western punctuation) → Rotated
  return VerticalOrientation::Rotated;
}

PunctuationTransform VerticalTextUtils::getPunctuationTransform(int32_t c) {
  switch (c) {
    // === Rotate 90° class ===
    // Paired brackets — rotate to match vertical orientation
    case 0xFF08:  // （ fullwidth left paren
    case 0xFF09:  // ） fullwidth right paren
    case 0x3008:  // 〈 left angle bracket
    case 0x3009:  // 〉 right angle bracket
    case 0x300A:  // 《 left double angle bracket
    case 0x300B:  // 》 right double angle bracket
    case 0x300C:  // 「 left corner bracket
    case 0x300D:  // 」 right corner bracket
    case 0x300E:  // 『 left white corner bracket
    case 0x300F:  // 』 right white corner bracket
    case 0x3010:  // 【 left black lenticular bracket
    case 0x3011:  // 】 right black lenticular bracket
    case 0x3014:  // 〔 left tortoise shell bracket
    case 0x3015:  // 〕 right tortoise shell bracket
    case 0x3016:  // 〖 left white lenticular bracket
    case 0x3017:  // 〗 right white lenticular bracket
    case 0x3018:  // 〘 left white tortoise shell bracket
    case 0x3019:  // 〙 right white tortoise shell bracket
    case 0x301A:  // 〚 left white square bracket
    case 0x301B:  // 〛 right white square bracket
    case 0xFF3B:  // ［ fullwidth left square bracket
    case 0xFF3D:  // ］ fullwidth right square bracket
    case 0xFF5B:  // ｛ fullwidth left curly bracket
    case 0xFF5D:  // ｝ fullwidth right curly bracket
    // Quotation marks
    case 0x2018:  // ' left single quotation
    case 0x2019:  // ' right single quotation
    case 0x201C:  // " left double quotation
    case 0x201D:  // " right double quotation
    // Dashes and ellipsis — rotate horizontal line to vertical
    case 0x2014:  // — em dash
    case 0x2015:  // ― horizontal bar
    case 0x2026:  // … horizontal ellipsis
    case 0xFF5E:  // ～ fullwidth tilde
    case 0x30FC:  // ー katakana prolonged sound mark
    case 0x2013:  // – en dash
      return PunctuationTransform::Rotate90;

    // === Offset class ===
    // These punctuation marks need position offset (from lower-left to upper-right area)
    case 0xFF0C:  // ， fullwidth comma
    case 0x3002:  // 。 ideographic full stop
    case 0x3001:  // 、 ideographic comma
    case 0xFF1A:  // ： fullwidth colon
    case 0xFF1B:  // ； fullwidth semicolon

      return PunctuationTransform::Offset;

    // === No transform class ===
    // These characters are visually symmetric or already appropriate for vertical display
    case 0xFF01:  // ！ fullwidth exclamation
    case 0xFF1F:  // ？ fullwidth question
    case 0x30FB:  // ・ katakana middle dot
    case 0x00B7:  // · middle dot
      return PunctuationTransform::None;

    default:
      return PunctuationTransform::None;
  }
}

void VerticalTextUtils::getPunctuationOffset(int32_t /*unichar*/, float fontSize, float* outDx,
                                             float* outDy) {
  // Offset punctuation from the horizontal position (lower-left) to vertical position
  // (upper-right) within the character cell. The offset is proportional to fontSize.
  // These values approximate the behavior in Adobe After Effects.
  if (outDx) {
    *outDx = fontSize * 0.5f;
  }
  if (outDy) {
    *outDy = -fontSize * 0.5f;
  }
}

}  // namespace pagx
