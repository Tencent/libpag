/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
//  in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/layers/TextLayouter.h"
#include <unordered_map>
#include "tgfx/core/TextBlobBuilder.h"
#include "tgfx/core/Typeface.h"
#include "tgfx/core/UTF.h"

namespace pagx {

using namespace tgfx;

struct GlyphInfo {
  Unichar unichar = 0;
  GlyphID glyphID = 0;
  std::shared_ptr<Typeface> typeface = nullptr;
};

static std::mutex& FallbackMutex = *new std::mutex;
static std::vector<std::shared_ptr<Typeface>> FallbackTypefaces = {};

void TextLayouter::SetFallbackTypefaces(std::vector<std::shared_ptr<Typeface>> typefaces) {
  std::lock_guard<std::mutex> lock(FallbackMutex);
  FallbackTypefaces = std::move(typefaces);
}

std::vector<std::shared_ptr<Typeface>> TextLayouter::GetFallbackTypefaces() {
  std::lock_guard<std::mutex> lock(FallbackMutex);
  return FallbackTypefaces;
}

static std::vector<GlyphInfo> ShapeText(const std::string& text,
                                        const std::shared_ptr<Typeface>& typeface,
                                        const std::vector<std::shared_ptr<Typeface>>& fallbacks) {
  if (text.empty()) {
    return {};
  }

  std::vector<GlyphInfo> glyphs = {};
  glyphs.reserve(text.size());

  const char* head = text.data();
  const char* tail = head + text.size();

  while (head < tail) {
    auto unichar = UTF::NextUTF8(&head, tail);

    GlyphID glyphID = typeface ? typeface->getGlyphID(unichar) : 0;
    std::shared_ptr<Typeface> matchedTypeface = typeface;

    if (glyphID == 0) {
      for (const auto& fallback : fallbacks) {
        if (fallback == nullptr) {
          continue;
        }
        glyphID = fallback->getGlyphID(unichar);
        if (glyphID > 0) {
          matchedTypeface = fallback;
          break;
        }
      }
    }

    glyphs.push_back({unichar, glyphID, matchedTypeface});
  }

  return glyphs;
}

std::shared_ptr<TextBlob> TextLayouter::Layout(const std::string& text, const Font& font) {
  if (text.empty()) {
    return nullptr;
  }

  auto typeface = font.getTypeface();
  auto fallbacks = GetFallbackTypefaces();

  // If primary typeface is empty or invalid, try to use first fallback or system default
  if (typeface == nullptr || typeface->fontFamily().empty()) {
    if (!fallbacks.empty() && fallbacks[0] != nullptr) {
      typeface = fallbacks[0];
    } else {
      // Try system default fonts (platform-specific)
      static const char* defaultFonts[] = {"Helvetica", "Arial", "sans-serif", nullptr};
      for (int i = 0; defaultFonts[i] != nullptr && typeface == nullptr; i++) {
        typeface = Typeface::MakeFromName(defaultFonts[i], "");
      }
      if (typeface == nullptr) {
        return nullptr;
      }
    }
  }

  auto glyphs = ShapeText(text, typeface, fallbacks);
  if (glyphs.empty()) {
    return nullptr;
  }

  std::vector<Point> positions = {};
  positions.reserve(glyphs.size());

  float xOffset = 0;
  float emptyAdvance = font.getSize() / 2.0f;

  for (const auto& glyph : glyphs) {
    positions.push_back({xOffset, 0});

    if (glyph.glyphID > 0 && glyph.typeface != nullptr) {
      Font glyphFont = font;
      glyphFont.setTypeface(glyph.typeface);
      xOffset += glyphFont.getAdvance(glyph.glyphID);
    } else {
      xOffset += emptyAdvance;
    }
  }

  // Group glyphs by typeface
  std::unordered_map<uint32_t, std::vector<size_t>> typefaceToIndices = {};
  for (size_t i = 0; i < glyphs.size(); i++) {
    const auto& glyph = glyphs[i];
    if (glyph.glyphID == 0 || glyph.typeface == nullptr) {
      continue;
    }
    typefaceToIndices[glyph.typeface->uniqueID()].push_back(i);
  }

  if (typefaceToIndices.empty()) {
    return nullptr;
  }

  TextBlobBuilder builder;
  for (const auto& [typefaceID, indices] : typefaceToIndices) {
    if (indices.empty()) {
      continue;
    }

    auto typeface = glyphs[indices[0]].typeface;
    Font runFont = font;
    runFont.setTypeface(typeface);

    const auto& buffer = builder.allocRunPos(runFont, indices.size());
    auto* pointPositions = reinterpret_cast<Point*>(buffer.positions);

    for (size_t i = 0; i < indices.size(); i++) {
      auto idx = indices[i];
      buffer.glyphs[i] = glyphs[idx].glyphID;
      pointPositions[i] = positions[idx];
    }
  }

  return builder.build();
}

}  // namespace pagx
