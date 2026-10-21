/////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Tencent. All rights reserved.
// Licensed under the Apache License, Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0).
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pagx {

struct TextCluster {
  size_t start = 0;
  size_t end = 0;
  std::vector<int32_t> codepoints = {};
};

// Unicode 16.0 extended grapheme clusters (UAX #29), with UTF-8 byte offsets.
std::vector<TextCluster> SegmentTextClusters(const std::string& text);
bool IsEmojiCharacter(int32_t codepoint);
bool IsEmojiPresentation(int32_t codepoint);
bool IsEmojiVariationBase(int32_t codepoint);
bool IsEmojiModifier(int32_t codepoint);
bool IsRegionalIndicator(int32_t codepoint);
bool IsDefaultIgnorable(int32_t codepoint);

}  // namespace pagx
