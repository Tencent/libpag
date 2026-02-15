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
  // CJK Symbols and Punctuation (includes some ideographic chars)
  if (c >= 0x3000 && c <= 0x303F) return true;
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
  return c == ' ' || c == '\t' || c == 0x00A0 /* NBSP */ || c == 0x3000 /* ideographic space */;
}

bool LineBreaker::isLineStartProhibited(int32_t c) {
  // Characters that must not appear at the start of a line (kinsoku)
  switch (c) {
    // CJK punctuation - closing/trailing
    case 0x3001:  // 、 ideographic comma
    case 0x3002:  // 。 ideographic period
    case 0xFF0C:  // ， fullwidth comma
    case 0xFF0E:  // ． fullwidth period
    case 0xFF01:  // ！ fullwidth exclamation
    case 0xFF1F:  // ？ fullwidth question
    case 0xFF1A:  // ： fullwidth colon
    case 0xFF1B:  // ； fullwidth semicolon
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
    case 0x301C:  // 〜 wave dash
    case 0x30FB:  // ・ katakana middle dot
    case 0x30FC:  // ー katakana-hiragana prolonged sound
    case 0x30FD:  // ヽ katakana iteration mark
    case 0x30FE:  // ヾ katakana voiced iteration mark
    case 0x309D:  // ゝ hiragana iteration mark
    case 0x309E:  // ゞ hiragana voiced iteration mark
    case 0x2019:  // ' right single quotation
    case 0x201D:  // " right double quotation
    // Small kana
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
  // Characters that must not appear at the end of a line (kinsoku)
  switch (c) {
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
      return true;
    default:
      return false;
  }
}

bool LineBreaker::isBreakAfter(int32_t c) {
  // Characters after which a line break is allowed
  return c == '-' || c == 0x2010 /* hyphen */ || c == 0x2012 /* figure dash */ ||
         c == 0x2013 /* en dash */;
}

bool LineBreaker::canBreakBetween(int32_t prevChar, int32_t nextChar) {
  if (prevChar == 0 || nextChar == 0) {
    return false;
  }

  // Never break before or after newline (handled separately)
  if (prevChar == '\n' || nextChar == '\n') {
    return false;
  }

  // Check kinsoku: don't break if nextChar is prohibited at line start
  if (isLineStartProhibited(nextChar)) {
    return false;
  }

  // Check kinsoku: don't break if prevChar is prohibited at line end
  if (isLineEndProhibited(prevChar)) {
    return false;
  }

  // Break after whitespace
  if (isWhitespace(prevChar)) {
    return true;
  }

  // Break after hyphen-like characters
  if (isBreakAfter(prevChar)) {
    return true;
  }

  // Break before CJK character (unless prohibited)
  if (isCJK(nextChar)) {
    return true;
  }

  // Break after CJK character (unless prohibited)
  if (isCJK(prevChar)) {
    return true;
  }

  return false;
}

}  // namespace pagx
