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

#include "TextShaperPrimitive.h"
#include "rendering/FontManager.h"
#include "tgfx/core/UTF.h"

namespace pag {
std::vector<ShapedGlyph> TextShaperPrimitive::Shape(const std::string& text,
                                                    std::shared_ptr<tgfx::Typeface> typeface) {
  const char* textStart = text.data();
  const char* textStop = textStart + text.size();
  std::vector<ShapedGlyph> glyphs = {};
  auto fallbackTypefaces = FontManager::GetFallbackTypefaces();
  while (textStart < textStop) {
    auto oldPosition = textStart;
    tgfx::UTF::NextUTF8(&textStart, textStop);
    auto length = textStart - oldPosition;
    auto str = std::string(oldPosition, length);
    auto glyphID = typeface ? typeface->getGlyphID(str) : 0;
    if (glyphID == 0) {
      for (const auto& faceHolder : fallbackTypefaces) {
        auto face = faceHolder->getTypeface();
        if (face == nullptr) {
          continue;
        }
        glyphID = face->getGlyphID(str);
        if (glyphID != 0) {
          glyphs.emplace_back(std::move(face), glyphID, oldPosition - text.data());
          break;
        }
      }
    } else {
      glyphs.emplace_back(typeface, glyphID, oldPosition - text.data());
    }
  }
  return glyphs;
}
}  // namespace pag
