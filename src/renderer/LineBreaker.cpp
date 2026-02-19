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

#include "LineBreaker.h"

namespace pagx {

bool LineBreaker::isCJK(int32_t c) {
  // CJK Unified Ideographs
  if (c >= 0x4E00 && c <= 0x9FFF) return true;
  // CJK Unified Ideographs Extension A
  if (c >= 0x3400 && c <= 0x4DBF) return true;
  // CJK Unified Ideographs Extension B
  if (c >= 0x20000 && c <= 0x2A6DF) return true;
  // CJK Unified Ideographs Extension C
  if (c >= 0x2A700 && c <= 0x2B73F) return true;
  // CJK Unified Ideographs Extension D
  if (c >= 0x2B740 && c <= 0x2B81F) return true;
  // CJK Unified Ideographs Extension E
  if (c >= 0x2B820 && c <= 0x2CEAF) return true;
  // CJK Unified Ideographs Extension F
  if (c >= 0x2CEB0 && c <= 0x2EBEF) return true;
  // CJK Unified Ideographs Extension G
  if (c >= 0x30000 && c <= 0x3134F) return true;
  // CJK Unified Ideographs Extension H
  if (c >= 0x31350 && c <= 0x323AF) return true;
  // CJK Unified Ideographs Extension I
  if (c >= 0x2EBF0 && c <= 0x2F7FF) return true;
  // CJK Compatibility Ideographs
  if (c >= 0xF900 && c <= 0xFAFF) return true;
  // CJK Compatibility Ideographs Supplement
  if (c >= 0x2F800 && c <= 0x2FA1F) return true;
  // Hiragana
  if (c >= 0x3040 && c <= 0x309F) return true;
  // Katakana
  if (c >= 0x30A0 && c <= 0x30FF) return true;
  // Katakana Phonetic Extensions
  if (c >= 0x31F0 && c <= 0x31FF) return true;
  // Hangul Syllables
  if (c >= 0xAC00 && c <= 0xD7AF) return true;
  // Hangul Jamo
  if (c >= 0x1100 && c <= 0x11FF) return true;
  // Hangul Compatibility Jamo
  if (c >= 0x3130 && c <= 0x318F) return true;
  // CJK Radicals Supplement
  if (c >= 0x2E80 && c <= 0x2EFF) return true;
  // Kangxi Radicals
  if (c >= 0x2F00 && c <= 0x2FDF) return true;
  // CJK Symbols and Punctuation
  if (c >= 0x3000 && c <= 0x303F) return true;
  // Bopomofo
  if (c >= 0x3100 && c <= 0x312F) return true;
  // Bopomofo Extended
  if (c >= 0x31A0 && c <= 0x31BF) return true;
  // Enclosed CJK Letters and Months
  if (c >= 0x3200 && c <= 0x32FF) return true;
  // CJK Compatibility
  if (c >= 0x3300 && c <= 0x33FF) return true;
  // Fullwidth Latin letters and Halfwidth Katakana (Fullwidth Forms)
  if (c >= 0xFF01 && c <= 0xFF60) return true;
  // Halfwidth Katakana
  if (c >= 0xFF65 && c <= 0xFF9F) return true;
  // Yi Syllables and Radicals
  if (c >= 0xA000 && c <= 0xA4CF) return true;
  return false;
}

bool LineBreaker::isWhitespace(int32_t c) {
  return c == ' ' || c == '\t' || c == 0x00A0 || c == 0x3000;
}

static bool isBreakableWhitespace(int32_t c) {
  return c == ' ' || c == '\t' || c == 0x3000;
}

bool LineBreaker::isLineStartProhibited(int32_t c) {
  switch (c) {
    // Closing brackets (JLREQ cl-02)
    case 0xFF09:  // ） fullwidth right paren
    case 0x3009:  // 〉 right angle bracket
    case 0x300B:  // 》 right double angle bracket
    case 0x300D:  // 」 right corner bracket
    case 0x300F:  // 』 right white corner bracket
    case 0x3011:  // 】 right black lenticular bracket
    case 0x3015:  // 〕 right tortoise shell bracket
    case 0x3017:  // 〗 right white lenticular bracket
    case 0x3019:  // 〙 right white tortoise shell bracket
    case 0x301B:  // 〛 right white square bracket
    case 0xFF3D:  // ］ fullwidth right square bracket
    case 0xFF5D:  // ｝ fullwidth right curly bracket
    case 0x2019:  // ' right single quotation
    case 0x201D:  // " right double quotation
    // Hyphens (JLREQ cl-03)
    case 0xFF0D:  // － fullwidth hyphen-minus
    case 0x2013:  // – en dash
    // Dividing punctuation (JLREQ cl-04)
    case 0xFF01:  // ！ fullwidth exclamation
    case 0xFF1F:  // ？ fullwidth question
    // Middle dots (JLREQ cl-05)
    case 0x30FB:  // ・ katakana middle dot
    case 0xFF65:  // ･ halfwidth katakana middle dot
    case 0x00B7:  // · middle dot
    // Full stops (JLREQ cl-06)
    case 0x3002:  // 。 ideographic period
    case 0xFF0E:  // ． fullwidth period
    // Commas (JLREQ cl-07)
    case 0x3001:  // 、 ideographic comma
    case 0xFF0C:  // ， fullwidth comma
    // Colons and semicolons
    case 0xFF1A:  // ： fullwidth colon
    case 0xFF1B:  // ； fullwidth semicolon
    // Inseparable characters (JLREQ cl-08)
    case 0x2026:  // … horizontal ellipsis
    case 0x2025:  // ‥ two dot leader
    case 0x2014:  // — em dash
    case 0x2E3A:  // ⸺ two-em dash
    case 0x301C:  // 〜 wave dash
    // Prolonged sound mark (JLREQ cl-10)
    case 0x30FC:  // ー katakana-hiragana prolonged sound
    // Iteration marks (JLREQ cl-09)
    case 0x30FD:  // ヽ katakana iteration mark
    case 0x30FE:  // ヾ katakana voiced iteration mark
    case 0x309D:  // ゝ hiragana iteration mark
    case 0x309E:  // ゞ hiragana voiced iteration mark
    // Small kana (JLREQ cl-11)
    case 0x3041:  // ぁ
    case 0x3043:  // ぃ
    case 0x3045:  // ぅ
    case 0x3047:  // ぇ
    case 0x3049:  // ぉ
    case 0x3063:  // っ
    case 0x3083:  // ゃ
    case 0x3085:  // ゅ
    case 0x3087:  // ょ
    case 0x308E:  // ゎ
    case 0x30A1:  // ァ
    case 0x30A3:  // ィ
    case 0x30A5:  // ゥ
    case 0x30A7:  // ェ
    case 0x30A9:  // ォ
    case 0x30C3:  // ッ
    case 0x30E3:  // ャ
    case 0x30E5:  // ュ
    case 0x30E7:  // ョ
    case 0x30EE:  // ヮ
    case 0x30F5:  // ヵ
    case 0x30F6:  // ヶ
      return true;
    default:
      return false;
  }
}

bool LineBreaker::isLineEndProhibited(int32_t c) {
  switch (c) {
    // Opening brackets (JLREQ cl-01)
    case 0xFF08:  // （ fullwidth left paren
    case 0x3008:  // 〈 left angle bracket
    case 0x300A:  // 《 left double angle bracket
    case 0x300C:  // 「 left corner bracket
    case 0x300E:  // 『 left white corner bracket
    case 0x3010:  // 【 left black lenticular bracket
    case 0x3014:  // 〔 left tortoise shell bracket
    case 0x3016:  // 〖 left white lenticular bracket
    case 0x3018:  // 〘 left white tortoise shell bracket
    case 0x301A:  // 〚 left white square bracket
    case 0xFF3B:  // ［ fullwidth left square bracket
    case 0xFF5B:  // ｛ fullwidth left curly bracket
    case 0x2018:  // ' left single quotation
    case 0x201C:  // " left double quotation
    // Currency symbols (should not be separated from the following number)
    case 0xFF04:  // ＄ fullwidth dollar sign
    case 0xFFE5:  // ￥ fullwidth yen sign
    case 0xFFE1:  // ￡ fullwidth pound sign
      return true;
    default:
      return false;
  }
}

bool LineBreaker::isBreakAfter(int32_t c) {
  return c == '-' || c == 0x2010 || c == 0x2012 || c == 0x2013 || c == 0x00AD;
}

bool LineBreaker::isEmojiComponent(int32_t c) {
  // ZWJ (Zero Width Joiner)
  if (c == 0x200D) return true;
  // Variation Selector 16 (emoji presentation)
  if (c == 0xFE0F) return true;
  // Combining Enclosing Keycap
  if (c == 0x20E3) return true;
  // Emoji Modifiers (skin tones)
  if (c >= 0x1F3FB && c <= 0x1F3FF) return true;
  // Regional Indicator Symbols
  if (c >= 0x1F1E0 && c <= 0x1F1FF) return true;
  // Tag Characters (used in flag sequences)
  if (c >= 0xE0020 && c <= 0xE007F) return true;
  // Variation Selector 15 (text presentation)
  if (c == 0xFE0E) return true;
  return false;
}

bool LineBreaker::isSoftHyphen(int32_t c) {
  return c == 0x00AD;
}

bool LineBreaker::canBreakBetween(int32_t prevChar, int32_t nextChar) {
  if (prevChar == 0 || nextChar == 0) {
    return false;
  }

  if (prevChar == '\n' || nextChar == '\n') {
    return false;
  }

  if (isEmojiComponent(nextChar)) {
    return false;
  }

  if (isLineStartProhibited(nextChar)) {
    return false;
  }

  if (isLineEndProhibited(prevChar)) {
    return false;
  }

  if (isSoftHyphen(prevChar)) {
    return true;
  }

  if (isBreakableWhitespace(prevChar)) {
    return true;
  }

  if (isBreakAfter(prevChar)) {
    return true;
  }

  if (isCJK(nextChar)) {
    return true;
  }

  if (isCJK(prevChar)) {
    return true;
  }

  return false;
}

}  // namespace pagx
