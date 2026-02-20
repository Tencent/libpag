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
#include <cstddef>

namespace pagx {

// ---------------------------------------------------------------------------
// UAX#14 pair table actions
// ---------------------------------------------------------------------------
enum class BreakAction : uint8_t {
  Direct = 0,    // D: direct break allowed
  Indirect = 1,  // I: break allowed only if spaces intervene
  Prohibit = 2,  // P: no break allowed
};

static constexpr auto D = BreakAction::Direct;
static constexpr auto I = BreakAction::Indirect;
static constexpr auto P = BreakAction::Prohibit;

// Pair table indexed by [prev_class][next_class].
// Rows: OP CL QU NS EX GL BA BB IN PR PO NU AL ID CM SP
// clang-format off
static const BreakAction PairTable[16][16] = {
//       OP  CL  QU  NS  EX  GL  BA  BB  IN  PR  PO  NU  AL  ID  CM  SP
/*OP*/ { P,  P,  P,  P,  P,  P,  P,  P,  P,  P,  P,  P,  P,  P,  P,  P },
/*CL*/ { D,  P,  I,  P,  P,  I,  I,  D,  D,  D,  D,  D,  D,  D,  P,  I },
/*QU*/ { P,  P,  I,  I,  P,  I,  I,  I,  I,  I,  I,  I,  I,  I,  P,  I },
/*NS*/ { D,  P,  I,  I,  P,  I,  I,  D,  D,  D,  D,  D,  D,  D,  P,  I },
/*EX*/ { D,  P,  I,  P,  P,  I,  I,  D,  D,  D,  D,  D,  D,  D,  P,  I },
/*GL*/ { I,  P,  I,  I,  P,  I,  I,  I,  I,  I,  I,  I,  I,  I,  P,  I },
/*BA*/ { D,  P,  I,  I,  P,  I,  I,  D,  D,  D,  D,  D,  D,  D,  P,  I },
/*BB*/ { D,  P,  I,  I,  P,  I,  I,  D,  D,  D,  D,  D,  D,  D,  P,  I },
/*IN*/ { D,  P,  I,  I,  P,  I,  I,  D,  I,  D,  D,  D,  D,  D,  P,  I },
/*PR*/ { I,  P,  I,  I,  P,  I,  I,  D,  D,  I,  I,  I,  I,  I,  P,  I },
/*PO*/ { I,  P,  I,  I,  P,  I,  I,  D,  D,  D,  I,  I,  I,  D,  P,  I },
/*NU*/ { I,  P,  I,  I,  P,  I,  I,  D,  I,  I,  I,  I,  I,  D,  P,  I },
/*AL*/ { I,  P,  I,  I,  P,  I,  I,  D,  I,  I,  I,  I,  I,  D,  P,  I },
/*ID*/ { D,  P,  I,  I,  P,  I,  I,  D,  I,  D,  I,  D,  D,  D,  P,  I },
/*CM*/ { D,  P,  I,  I,  P,  I,  I,  D,  D,  D,  D,  D,  D,  D,  P,  I },
/*SP*/ { D,  P,  I,  I,  P,  I,  I,  D,  D,  D,  D,  D,  D,  D,  P,  I },
};
// clang-format on

// ---------------------------------------------------------------------------
// Character-to-class range table.
// Sorted by start code point. Characters not listed default to AL.
// ---------------------------------------------------------------------------
struct LineBreakRange {
  int32_t start;
  int32_t end;
  LineBreakClass lbClass;
};

using LBC = LineBreakClass;

// Sorted by start code point. No overlapping ranges. Binary search requires strict ordering.
// clang-format off
static const LineBreakRange Ranges[] = {
    // --- ASCII ---
    {0x0009, 0x0009, LBC::SP},    // tab
    {0x0020, 0x0020, LBC::SP},    // space
    {0x0021, 0x0021, LBC::EX},    // ! exclamation mark
    {0x0022, 0x0022, LBC::QU},    // " quotation mark
    {0x0024, 0x0024, LBC::PR},    // $ dollar sign
    {0x0025, 0x0025, LBC::PO},    // % percent sign
    {0x0027, 0x0027, LBC::QU},    // ' apostrophe
    {0x0028, 0x0028, LBC::OP},    // ( left parenthesis
    {0x0029, 0x0029, LBC::CL},    // ) right parenthesis
    {0x002C, 0x002C, LBC::BA},    // , comma (break after)
    {0x002D, 0x002D, LBC::BA},    // - hyphen-minus
    {0x002F, 0x002F, LBC::BA},    // / solidus (break after)
    {0x0030, 0x0039, LBC::NU},    // 0..9 ASCII digits
    {0x003A, 0x003B, LBC::BA},    // : ; colon, semicolon
    {0x003F, 0x003F, LBC::EX},    // ? question mark
    {0x005B, 0x005B, LBC::OP},    // [ left square bracket
    {0x005D, 0x005D, LBC::CL},    // ] right square bracket
    {0x007B, 0x007B, LBC::OP},    // { left curly bracket
    {0x007D, 0x007D, LBC::CL},    // } right curly bracket

    // --- Latin-1 Supplement ---
    {0x00A0, 0x00A0, LBC::GL},    // non-breaking space
    {0x00A2, 0x00A2, LBC::PO},    // ¢ cent sign (postfix)
    {0x00A3, 0x00A3, LBC::PR},    // £ pound sign
    {0x00A5, 0x00A5, LBC::PR},    // ¥ yen sign
    {0x00AB, 0x00AB, LBC::QU},    // « left guillemet
    {0x00AD, 0x00AD, LBC::BA},    // soft hyphen
    {0x00B7, 0x00B7, LBC::NS},    // · middle dot (non-starter)
    {0x00BB, 0x00BB, LBC::QU},    // » right guillemet

    // --- Hangul Jamo (ID) ---
    {0x1100, 0x11FF, LBC::ID},    // Hangul Jamo

    // --- General Punctuation ---
    {0x200B, 0x200B, LBC::BA},    // zero-width space (break opportunity)
    {0x200D, 0x200D, LBC::CM},    // zero-width joiner (emoji glue)
    {0x2010, 0x2010, LBC::BA},    // ‐ hyphen
    {0x2012, 0x2013, LBC::BA},    // ‒ figure dash, – en dash
    {0x2014, 0x2014, LBC::IN},    // — em dash (inseparable, kinsoku cl-08)
    {0x2018, 0x2018, LBC::QU},    // ' left single quote
    {0x2019, 0x2019, LBC::QU},    // ' right single quote
    {0x201C, 0x201C, LBC::QU},    // " left double quote
    {0x201D, 0x201D, LBC::QU},    // " right double quote
    {0x2025, 0x2026, LBC::IN},    // ‥ two dot leader, … horizontal ellipsis
    {0x2039, 0x2039, LBC::QU},    // ‹ single left-pointing angle quote
    {0x203A, 0x203A, LBC::QU},    // › single right-pointing angle quote
    {0x2060, 0x2060, LBC::GL},    // word joiner (no break)

    // --- Letterlike Symbols ---
    {0x2103, 0x2103, LBC::PO},    // ℃ degree celsius (postfix)
    {0x2109, 0x2109, LBC::PO},    // ℉ degree fahrenheit (postfix)

    // --- Currency Symbols block ---
    {0x20A0, 0x20CF, LBC::PR},    // currency symbols block (€ etc.)
    {0x20E3, 0x20E3, LBC::CM},    // combining enclosing keycap

    // --- Miscellaneous Symbols (ID) ---
    {0x2600, 0x26FF, LBC::ID},    // Miscellaneous Symbols
    {0x2700, 0x27BF, LBC::ID},    // Dingbats

    // --- CJK Radicals (ID) ---
    {0x2E3A, 0x2E3A, LBC::IN},    // ⸺ two-em dash (inseparable)
    {0x2E80, 0x2EFF, LBC::ID},    // CJK Radicals Supplement
    {0x2F00, 0x2FDF, LBC::ID},    // Kangxi Radicals

    // --- CJK Symbols and Punctuation (0x3000-0x303F) ---
    {0x3000, 0x3000, LBC::SP},    // ideographic space
    {0x3001, 0x3001, LBC::CL},    // 、 ideographic comma
    {0x3002, 0x3002, LBC::CL},    // 。 ideographic full stop
    {0x3005, 0x3005, LBC::NS},    // 々 ideographic iteration mark
    {0x3008, 0x3008, LBC::OP},    // 〈 left angle bracket
    {0x3009, 0x3009, LBC::CL},    // 〉 right angle bracket
    {0x300A, 0x300A, LBC::OP},    // 《 left double angle bracket
    {0x300B, 0x300B, LBC::CL},    // 》 right double angle bracket
    {0x300C, 0x300C, LBC::OP},    // 「 left corner bracket
    {0x300D, 0x300D, LBC::CL},    // 」 right corner bracket
    {0x300E, 0x300E, LBC::OP},    // 『 left white corner bracket
    {0x300F, 0x300F, LBC::CL},    // 』 right white corner bracket
    {0x3010, 0x3010, LBC::OP},    // 【 left black lenticular bracket
    {0x3011, 0x3011, LBC::CL},    // 】 right black lenticular bracket
    {0x3014, 0x3014, LBC::OP},    // 〔 left tortoise shell bracket
    {0x3015, 0x3015, LBC::CL},    // 〕 right tortoise shell bracket
    {0x3016, 0x3016, LBC::OP},    // 〖 left white lenticular bracket
    {0x3017, 0x3017, LBC::CL},    // 〗 right white lenticular bracket
    {0x3018, 0x3018, LBC::OP},    // 〘 left white tortoise shell bracket
    {0x3019, 0x3019, LBC::CL},    // 〙 right white tortoise shell bracket
    {0x301A, 0x301A, LBC::OP},    // 〚 left white square bracket
    {0x301B, 0x301B, LBC::CL},    // 〛 right white square bracket
    {0x301C, 0x301C, LBC::NS},    // 〜 wave dash (kinsoku cl-08)

    // --- Hiragana (0x3040-0x309F): NS for small kana/iteration, ID for rest ---
    {0x3041, 0x3041, LBC::NS},    // ぁ
    {0x3042, 0x3042, LBC::ID},    // あ
    {0x3043, 0x3043, LBC::NS},    // ぃ
    {0x3044, 0x3044, LBC::ID},    // い
    {0x3045, 0x3045, LBC::NS},    // ぅ
    {0x3046, 0x3046, LBC::ID},    // う
    {0x3047, 0x3047, LBC::NS},    // ぇ
    {0x3048, 0x3048, LBC::ID},    // え
    {0x3049, 0x3049, LBC::NS},    // ぉ
    {0x304A, 0x3062, LBC::ID},    // お..で
    {0x3063, 0x3063, LBC::NS},    // っ
    {0x3064, 0x3082, LBC::ID},    // つ..も
    {0x3083, 0x3083, LBC::NS},    // ゃ
    {0x3084, 0x3084, LBC::ID},    // や
    {0x3085, 0x3085, LBC::NS},    // ゅ
    {0x3086, 0x3086, LBC::ID},    // ゆ
    {0x3087, 0x3087, LBC::NS},    // ょ
    {0x3088, 0x308D, LBC::ID},    // よ..ろ
    {0x308E, 0x308E, LBC::NS},    // ゎ
    {0x308F, 0x3096, LBC::ID},    // わ..ゖ
    {0x309D, 0x309E, LBC::NS},    // ゝ ゞ hiragana iteration marks
    {0x309F, 0x309F, LBC::ID},    // ゟ

    // --- Katakana (0x30A0-0x30FF): NS for small kana/iteration/marks, ID for rest ---
    {0x30A0, 0x30A0, LBC::ID},    // ゠ katakana-hiragana double hyphen
    {0x30A1, 0x30A1, LBC::NS},    // ァ
    {0x30A2, 0x30A2, LBC::ID},    // ア
    {0x30A3, 0x30A3, LBC::NS},    // ィ
    {0x30A4, 0x30A4, LBC::ID},    // イ
    {0x30A5, 0x30A5, LBC::NS},    // ゥ
    {0x30A6, 0x30A6, LBC::ID},    // ウ
    {0x30A7, 0x30A7, LBC::NS},    // ェ
    {0x30A8, 0x30A8, LBC::ID},    // エ
    {0x30A9, 0x30A9, LBC::NS},    // ォ
    {0x30AA, 0x30C2, LBC::ID},    // オ..チ
    {0x30C3, 0x30C3, LBC::NS},    // ッ
    {0x30C4, 0x30E2, LBC::ID},    // ツ..モ
    {0x30E3, 0x30E3, LBC::NS},    // ャ
    {0x30E4, 0x30E4, LBC::ID},    // ヤ
    {0x30E5, 0x30E5, LBC::NS},    // ュ
    {0x30E6, 0x30E6, LBC::ID},    // ユ
    {0x30E7, 0x30E7, LBC::NS},    // ョ
    {0x30E8, 0x30ED, LBC::ID},    // ヨ..ロ
    {0x30EE, 0x30EE, LBC::NS},    // ヮ
    {0x30EF, 0x30F4, LBC::ID},    // ワ..ヴ
    {0x30F5, 0x30F6, LBC::NS},    // ヵ ヶ
    {0x30F7, 0x30FA, LBC::ID},    // ヷ..ヺ
    {0x30FB, 0x30FB, LBC::NS},    // ・ katakana middle dot
    {0x30FC, 0x30FC, LBC::NS},    // ー prolonged sound mark
    {0x30FD, 0x30FE, LBC::NS},    // ヽ ヾ katakana iteration marks
    {0x30FF, 0x30FF, LBC::ID},    // ヿ

    // --- Bopomofo / CJK Ideographs / Compatibility ---
    {0x3100, 0x312F, LBC::ID},    // Bopomofo
    {0x3130, 0x318F, LBC::ID},    // Hangul Compatibility Jamo
    {0x31A0, 0x31BF, LBC::ID},    // Bopomofo Extended
    {0x31F0, 0x31FF, LBC::ID},    // Katakana Phonetic Extensions
    {0x3200, 0x32FF, LBC::ID},    // Enclosed CJK Letters and Months
    {0x3300, 0x33FF, LBC::ID},    // CJK Compatibility
    {0x3400, 0x4DBF, LBC::ID},    // CJK Extension A
    {0x4E00, 0x9FFF, LBC::ID},    // CJK Unified Ideographs
    {0xA000, 0xA4CF, LBC::ID},    // Yi Syllables and Radicals
    {0xAC00, 0xD7AF, LBC::ID},    // Hangul Syllables
    {0xF900, 0xFAFF, LBC::ID},    // CJK Compatibility Ideographs

    // --- Variation selectors / special ---
    {0xFE0E, 0xFE0F, LBC::CM},    // text/emoji variation selectors
    {0xFEFF, 0xFEFF, LBC::GL},    // BOM / zero-width no-break space

    // --- Fullwidth forms ---
    {0xFF01, 0xFF01, LBC::EX},    // ！ fullwidth exclamation
    {0xFF04, 0xFF04, LBC::PR},    // ＄ fullwidth dollar sign
    {0xFF05, 0xFF05, LBC::PO},    // ％ fullwidth percent sign
    {0xFF08, 0xFF08, LBC::OP},    // （ fullwidth left paren
    {0xFF09, 0xFF09, LBC::CL},    // ） fullwidth right paren
    {0xFF0C, 0xFF0C, LBC::CL},    // ， fullwidth comma
    {0xFF0D, 0xFF0D, LBC::BA},    // － fullwidth hyphen-minus
    {0xFF0E, 0xFF0E, LBC::CL},    // ． fullwidth full stop
    {0xFF10, 0xFF19, LBC::NU},    // ０..９ fullwidth digits
    {0xFF1A, 0xFF1A, LBC::NS},    // ： fullwidth colon (kinsoku)
    {0xFF1B, 0xFF1B, LBC::NS},    // ； fullwidth semicolon (kinsoku)
    {0xFF1F, 0xFF1F, LBC::EX},    // ？ fullwidth question mark
    {0xFF20, 0xFF3A, LBC::AL},    // fullwidth Latin capitals
    {0xFF3B, 0xFF3B, LBC::OP},    // ［ fullwidth left square bracket
    {0xFF3D, 0xFF3D, LBC::CL},    // ］ fullwidth right square bracket
    {0xFF41, 0xFF5A, LBC::AL},    // fullwidth Latin lowercase
    {0xFF5B, 0xFF5B, LBC::OP},    // ｛ fullwidth left curly bracket
    {0xFF5D, 0xFF5D, LBC::CL},    // ｝ fullwidth right curly bracket
    {0xFF5F, 0xFF5F, LBC::OP},    // ｟ fullwidth left white paren
    {0xFF60, 0xFF60, LBC::CL},    // ｠ fullwidth right white paren
    {0xFF65, 0xFF65, LBC::NS},    // ･ halfwidth katakana middle dot
    {0xFF66, 0xFF9F, LBC::ID},    // halfwidth Katakana
    {0xFFE0, 0xFFE0, LBC::PO},    // ￠ fullwidth cent sign
    {0xFFE1, 0xFFE1, LBC::PR},    // ￡ fullwidth pound sign
    {0xFFE5, 0xFFE5, LBC::PR},    // ￥ fullwidth yen sign

    // --- Supplementary planes ---
    // Emoji (split around CM sub-ranges to avoid overlap)
    {0x1F000, 0x1F0FF, LBC::ID},  // Mahjong, Domino, Playing Cards
    {0x1F1E0, 0x1F1FF, LBC::CM},  // Regional Indicator Symbols (flags)
    {0x1F300, 0x1F3FA, LBC::ID},  // Misc Symbols and Pictographs (before skin tones)
    {0x1F3FB, 0x1F3FF, LBC::CM},  // Emoji skin tone modifiers
    {0x1F400, 0x1F5FF, LBC::ID},  // Misc Symbols and Pictographs (after skin tones)
    {0x1F600, 0x1F64F, LBC::ID},  // Emoticons
    {0x1F680, 0x1F6FF, LBC::ID},  // Transport and Map Symbols
    {0x1F900, 0x1F9FF, LBC::ID},  // Supplemental Symbols and Pictographs
    {0x1FA00, 0x1FA6F, LBC::ID},  // Chess Symbols / Extended-A
    {0x1FA70, 0x1FAFF, LBC::ID},  // Extended-B

    // CJK Ideograph Extensions (supplementary planes)
    {0x20000, 0x2A6DF, LBC::ID},  // CJK Extension B
    {0x2A700, 0x2B73F, LBC::ID},  // CJK Extension C
    {0x2B740, 0x2B81F, LBC::ID},  // CJK Extension D
    {0x2B820, 0x2CEAF, LBC::ID},  // CJK Extension E
    {0x2CEB0, 0x2EBEF, LBC::ID},  // CJK Extension F
    {0x2EBF0, 0x2F7FF, LBC::ID},  // CJK Extension I
    {0x2F800, 0x2FA1F, LBC::ID},  // CJK Compatibility Ideographs Supplement
    {0x30000, 0x3134F, LBC::ID},  // CJK Extension G
    {0x31350, 0x323AF, LBC::ID},  // CJK Extension H

    // Tag characters (flag sequences)
    {0xE0020, 0xE007F, LBC::CM},  // Tag characters
};
// clang-format on

static constexpr size_t RangeCount = sizeof(Ranges) / sizeof(Ranges[0]);

// ---------------------------------------------------------------------------
// Sorted range table for binary search (built at startup or verified sorted).
// We keep the source table sorted manually and verify in debug builds.
// ---------------------------------------------------------------------------

bool LineBreaker::CanBreakBetween(int32_t prevChar, int32_t nextChar) {
  if (prevChar == 0 || nextChar == 0) {
    return false;
  }
  if (prevChar == '\n' || nextChar == '\n') {
    return false;
  }

  auto prevClass = GetLineBreakClass(prevChar);
  auto nextClass = GetLineBreakClass(nextChar);

  // CM inherits the class of its base character. If prevChar is CM, it was already attached to its
  // base, so treat it as AL for the purpose of the pair table lookup.
  if (prevClass == LBC::CM) {
    prevClass = LBC::AL;
  }
  // If nextChar is CM, no break before combining marks (pair table CM column is all P except SP).
  if (nextClass == LBC::CM) {
    return false;
  }

  auto action =
      PairTable[static_cast<int>(prevClass)][static_cast<int>(nextClass)];

  if (action == BreakAction::Direct) {
    return true;
  }
  if (action == BreakAction::Indirect) {
    // Indirect break: allowed only when the previous character is a space.
    return prevClass == LBC::SP;
  }
  return false;
}

LineBreakClass LineBreaker::GetLineBreakClass(int32_t c) {
  size_t lo = 0;
  size_t hi = RangeCount;
  while (lo < hi) {
    auto mid = lo + (hi - lo) / 2;
    if (c > Ranges[mid].end) {
      lo = mid + 1;
    } else if (c < Ranges[mid].start) {
      hi = mid;
    } else {
      return Ranges[mid].lbClass;
    }
  }
  return LBC::AL;
}

bool LineBreaker::IsCJK(int32_t c) {
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

bool LineBreaker::IsWhitespace(int32_t c) {
  return c == ' ' || c == '\t' || c == 0x00A0 || c == 0x3000;
}

}  // namespace pagx
