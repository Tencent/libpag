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
//  license is distributed on an "AS IS" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "EmojiPresentation.h"

namespace pagx {

namespace {

// The `Emoji_Presentation=Yes` set of Unicode 15.1 (emoji-data.txt), restricted to the codepoints
// that carry an emoji glyph by default rather than only when a U+FE0F asks for one. The
// supplementary pictograph blocks are given as whole ranges: no text face carries those
// codepoints, but listing them keeps the ZWJ continuation test below uniform across the planes.
struct EmojiRange {
  int32_t first;
  int32_t last;
};

const EmojiRange kDefaultEmojiPresentation[] = {
    {0x231A, 0x231B},   {0x23E9, 0x23EC},   {0x23F0, 0x23F0},   {0x23F3, 0x23F3},
    {0x25FD, 0x25FE},   {0x2614, 0x2615},   {0x2648, 0x2653},   {0x267F, 0x267F},
    {0x2693, 0x2693},   {0x26A1, 0x26A1},   {0x26AA, 0x26AB},   {0x26BD, 0x26BE},
    {0x26C4, 0x26C5},   {0x26CE, 0x26CE},   {0x26D4, 0x26D4},   {0x26EA, 0x26EA},
    {0x26F2, 0x26F3},   {0x26F5, 0x26F5},   {0x26FA, 0x26FA},   {0x26FD, 0x26FD},
    {0x2705, 0x2705},   {0x270A, 0x270B},   {0x2728, 0x2728},   {0x274C, 0x274C},
    {0x274E, 0x274E},   {0x2753, 0x2755},   {0x2757, 0x2757},   {0x2795, 0x2797},
    {0x27B0, 0x27B0},   {0x27BF, 0x27BF},   {0x2B1B, 0x2B1C},   {0x2B50, 0x2B50},
    {0x2B55, 0x2B55},   {0x1F004, 0x1F004}, {0x1F0CF, 0x1F0CF}, {0x1F18E, 0x1F18E},
    {0x1F191, 0x1F19A}, {0x1F1E6, 0x1F1FF}, {0x1F300, 0x1F5FF}, {0x1F600, 0x1F64F},
    {0x1F680, 0x1F6FF}, {0x1F7E0, 0x1F7EB}, {0x1F900, 0x1F9FF}, {0x1FA70, 0x1FAFF},
};

constexpr int32_t kVariationSelector15 = 0xFE0E;
constexpr int32_t kVariationSelector16 = 0xFE0F;
constexpr int32_t kZeroWidthJoiner = 0x200D;
constexpr int32_t kCombiningEnclosingKeycap = 0x20E3;
constexpr int32_t kSkinToneFirst = 0x1F3FB;
constexpr int32_t kSkinToneLast = 0x1F3FF;
constexpr int32_t kTagFirst = 0xE0020;
constexpr int32_t kTagLast = 0xE007F;

bool IsDefaultEmojiPresentation(int32_t codepoint) {
  if (codepoint < kDefaultEmojiPresentation[0].first) {
    return false;
  }
  for (const auto& range : kDefaultEmojiPresentation) {
    if (codepoint < range.first) {
      return false;
    }
    if (codepoint <= range.last) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool EmojiPresentation::IsEmojiBase(int32_t codepoint, int32_t nextCodepoint) {
  if (nextCodepoint == kVariationSelector16) {
    return true;
  }
  return IsDefaultEmojiPresentation(codepoint);
}

bool EmojiPresentation::IsEmojiSequenceMark(int32_t codepoint) {
  if (codepoint == kZeroWidthJoiner || codepoint == kCombiningEnclosingKeycap) {
    return true;
  }
  if (codepoint == kVariationSelector15 || codepoint == kVariationSelector16) {
    return true;
  }
  return (codepoint >= kSkinToneFirst && codepoint <= kSkinToneLast) ||
         (codepoint >= kTagFirst && codepoint <= kTagLast);
}

}  // namespace pagx
