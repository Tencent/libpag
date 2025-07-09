/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "NativeTextShaper.h"
#include "UnicodeEmojiTable.hh"
#include "rendering/FontManager.h"
#include "tgfx/core/UTF.h"

namespace pag {
template <typename T>
static inline bool InRange(T u, T lo, T hi) {
  return lo <= u && u <= hi;
}

static inline bool CodepointIsRegionalIndicator(int32_t u) {
  return InRange<int32_t>(u, 0x1F1E6u, 0x1F1FFu);
}

static const int32_t UNICODE_ZWJ = 0x200D;

struct Info {
  int32_t codepoint = 0;
  uint32_t cluster = 0;
  bool isContinuation = false;
};

static void CheckContinuations(std::vector<Info>& infos) {
  for (size_t i = 0; i < infos.size(); ++i) {
    if (infos[i].codepoint == 0x20E3 || infos[i].codepoint == 0xFE0F ||
        InRange(infos[i].codepoint, 0x1F3FB, 0x1F3FF) ||
        InRange(infos[i].codepoint, 0xE0020, 0xE007F)) {
      infos[i].isContinuation = true;
    } else if (i && CodepointIsRegionalIndicator(infos[i].codepoint)) {
      if (CodepointIsRegionalIndicator(infos[i - 1].codepoint) && !infos[i - 1].isContinuation) {
        infos[i].isContinuation = true;
      }
    } else if (infos[i].codepoint == UNICODE_ZWJ) {
      infos[i].isContinuation = true;
      if (i + 1 < infos.size() && EmojiIsExtendedPictographic(infos[i + 1].codepoint)) {
        i++;
        infos[i].isContinuation = true;
      }
    }
  }
}

static void MergeClusters(std::vector<Info>& infos) {
  auto groupEnd = [&](size_t start) -> size_t {
    while (++start < infos.size() && infos[start].isContinuation)
      ;
    return start;
  };

  for (size_t count = infos.size(), start = 0, end = count ? groupEnd(0) : 0; start < count;
       start = end, end = groupEnd(start)) {
    if (end - start < 2) {
      continue;
    }

    auto cluster = infos[start].cluster;

    for (size_t i = start + 1; i < end; i++) {
      cluster = std::min(cluster, infos[i].cluster);
    }

    /* Extend end */
    while (end < infos.size() && infos[end - 1].cluster == infos[end].cluster) {
      end++;
    }

    /* Extend start */
    while (0 < start && infos[start - 1].cluster == infos[start].cluster) {
      start--;
    }

    for (auto i = start; i < end; i++) {
      infos[i].cluster = cluster;
    }
  }
}

std::vector<ShapedGlyph> NativeTextShaper::Shape(const std::string& text,
                                                 std::shared_ptr<tgfx::Typeface> typeface) {
  std::vector<Info> infos = {};
  auto textStart = text.data();
  auto textStop = textStart + text.size();
  while (textStart < textStop) {
    auto cluster = static_cast<uint32_t>(textStart - text.data());
    infos.emplace_back(Info{tgfx::UTF::NextUTF8(&textStart, textStop), cluster});
  }

  CheckContinuations(infos);
  MergeClusters(infos);

  std::vector<ShapedGlyph> glyphs = {};
  auto fallbackTypefaces = FontManager::GetFallbackTypefaces();
  for (size_t i = 0; i < infos.size(); ++i) {
    auto length = (i + 1 == infos.size() ? text.length() : infos[i + 1].cluster) - infos[i].cluster;
    if (length == 0) {
      continue;
    }
    auto str = text.substr(infos[i].cluster, length);
    auto glyphID = typeface ? typeface->getGlyphID(str) : 0;
    if (glyphID == 0) {
      for (const auto& faceHolder : fallbackTypefaces) {
        auto face = faceHolder->getTypeface();
        glyphID = face->getGlyphID(str);
        if (glyphID != 0) {
          glyphs.emplace_back(std::move(face), glyphID, infos[i].cluster);
          break;
        }
      }
    } else {
      glyphs.emplace_back(typeface, typeface->getGlyphID(str), infos[i].cluster);
    }
  }
  return glyphs;
}
}  // namespace pag
