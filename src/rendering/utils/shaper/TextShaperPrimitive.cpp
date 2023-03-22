/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "TextShaperPrimitive.h"
#include "rendering/FontManager.h"
#include "tgfx/utils/UTF.h"

namespace pag {
PositionedGlyphs TextShaperPrimitive::Shape(const std::string& text,
                                            std::shared_ptr<tgfx::Typeface> typeface) {
  const char* textStart = &(text[0]);
  const char* textStop = textStart + text.size();
  std::vector<std::tuple<std::shared_ptr<tgfx::Typeface>, tgfx::GlyphID, uint32_t>> glyphs;
  auto fallbackTypefaces = FontManager::GetFallbackTypefaces();
  while (textStart < textStop) {
    auto oldPosition = textStart;
    tgfx::UTF::NextUTF8(&textStart, textStop);
    auto length = textStart - oldPosition;
    auto str = std::string(oldPosition, length);
    auto glyphID = typeface->getGlyphID(str);
    bool found = false;
    if (glyphID == 0) {
      for (const auto& faceHolder : fallbackTypefaces) {
        auto face = faceHolder->getTypeface();
        if (face == nullptr) {
          continue;
        }
        glyphID = face->getGlyphID(str);
        if (glyphID != 0) {
          glyphs.emplace_back(std::move(face), glyphID, oldPosition - &(text[0]));
          found = true;
          break;
        }
      }
      if (!found) {
        glyphs.emplace_back(typeface, glyphID, oldPosition - &(text[0]));
      }
    } else {
      glyphs.emplace_back(typeface, glyphID, oldPosition - &(text[0]));
    }
  }
  return PositionedGlyphs(glyphs);
}
}  // namespace pag
