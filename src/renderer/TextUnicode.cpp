/////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Tencent. All rights reserved.
// Licensed under the Apache License, Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0).
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "TextUnicode.h"
#include <algorithm>
#include <iterator>
#include "UTF8Utils.h"

namespace pagx {
namespace {
enum GraphemeProperty {
  Other,
  CR,
  LF,
  Control,
  Extend,
  ZWJ,
  RI,
  Prepend,
  SpacingMark,
  L,
  V,
  T,
  LV,
  LVT
};
enum Property {
  Emoji = 16,
  EmojiPresentation = 32,
  ExtendedPictographic = 64,
  IndicConsonant = 128,
  IndicLinker = 256,
  IndicExtend = 512,
  DefaultIgnorable = 1024,
  EmojiVariationBase = 2048,
  EmojiModifier = 4096
};
struct UnicodeRange {
  int32_t first;
  int32_t last;
  uint16_t properties;
};
#include "TextUnicodeData.h"

uint16_t Properties(int32_t codepoint) {
  auto it =
      std::lower_bound(std::begin(TextPropertyRanges), std::end(TextPropertyRanges), codepoint,
                       [](const auto& range, int32_t c) { return range.last < c; });
  uint16_t value =
      it != std::end(TextPropertyRanges) && it->first <= codepoint ? it->properties : 0;
  if (codepoint >= 0xAC00 && codepoint <= 0xD7A3) {
    value |= (codepoint - 0xAC00) % 28 == 0 ? LV : LVT;
  }
  return value;
}
bool IsControl(int property) {
  return property == CR || property == LF || property == Control;
}
}  // namespace

bool IsEmojiCharacter(int32_t codepoint) {
  return (Properties(codepoint) & Emoji) != 0;
}
bool IsEmojiPresentation(int32_t codepoint) {
  return (Properties(codepoint) & EmojiPresentation) != 0;
}
bool IsEmojiVariationBase(int32_t codepoint) {
  return (Properties(codepoint) & EmojiVariationBase) != 0;
}
bool IsEmojiModifier(int32_t codepoint) {
  return (Properties(codepoint) & EmojiModifier) != 0;
}
bool IsRegionalIndicator(int32_t codepoint) {
  return (Properties(codepoint) & 15) == RI;
}
bool IsDefaultIgnorable(int32_t codepoint) {
  return (Properties(codepoint) & DefaultIgnorable) != 0;
}

std::vector<TextCluster> SegmentTextClusters(const std::string& text) {
  std::vector<TextCluster> result;
  int previous = Other;
  size_t regionalCount = 0;
  bool pictographExtend = false, zwjAfterPictograph = false;
  bool indicConsonant = false, indicLinker = false;
  for (size_t pos = 0; pos < text.size();) {
    int32_t codepoint = 0;
    auto length = DecodeUTF8(text.data() + pos, text.size() - pos, &codepoint);
    if (length == 0) {
      length = 1;
      codepoint = 0xFFFD;
    }
    auto properties = Properties(codepoint);
    int current = properties & 15;
    bool joins = previous == CR && current == LF;  // GB3
    if (!IsControl(previous) && !IsControl(current)) {
      joins |= previous == L && (current == L || current == V || current == LV || current == LVT);
      joins |= (previous == LV || previous == V) && (current == V || current == T);
      joins |= (previous == LVT || previous == T) && current == T;
      joins |= current == Extend || current == ZWJ || current == SpacingMark || previous == Prepend;
      joins |= indicConsonant && indicLinker && (properties & IndicConsonant);
      joins |= zwjAfterPictograph && (properties & ExtendedPictographic);
      joins |= previous == RI && current == RI && regionalCount % 2 == 1;
    }
    if (result.empty() || !joins) result.push_back({pos, pos, {}});
    result.back().end = pos + length;
    result.back().codepoints.push_back(codepoint);
    zwjAfterPictograph = pictographExtend && current == ZWJ;
    pictographExtend =
        (properties & ExtendedPictographic) || (pictographExtend && current == Extend);
    if (properties & IndicConsonant) {
      indicConsonant = true;
      indicLinker = false;
    } else if (properties & (IndicExtend | IndicLinker)) {
      indicLinker |= (properties & IndicLinker) != 0;
    } else {
      indicConsonant = indicLinker = false;
    }
    regionalCount = current == RI ? regionalCount + 1 : 0;
    previous = current;
    pos += length;
  }
  return result;
}
}  // namespace pagx
