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

#include "Glyph.h"
#include "rendering/FontManager.h"
#include "tgfx/core/UTF.h"

namespace pag {
Glyph::Glyph(tgfx::GlyphID glyphId, std::string name, tgfx::Font font, bool isVertical)
    : _glyphId(glyphId), _name(std::move(name)), _font(std::move(font)), _isVertical(isVertical) {
  horizontalInfo->advance = _font.getGlyphAdvance(_glyphId);
  horizontalInfo->bounds = _font.getGlyphBounds(_glyphId);
  auto metrics = _font.getMetrics();
  horizontalInfo->ascent = metrics.ascent;
  horizontalInfo->descent = metrics.descent;
  if (_name == " ") {
    // 空格字符测量的 bounds 比较异常偏上，本身也不可见，这里直接按字幕 A 的上下边界调整一下。
    auto AGlyphID = _font.getGlyphID("A");
    if (AGlyphID > 0) {
      auto ABounds = _font.getGlyphBounds(AGlyphID);
      horizontalInfo->bounds.top = ABounds.top;
      horizontalInfo->bounds.bottom = ABounds.bottom;
    }
  }
  if (isVertical) {
    verticalInfo = std::make_shared<Info>(*horizontalInfo);
    if (_name.size() == 1) {
      // 字母，数字，标点等字符旋转 90° 绘制，原先的水平 baseline 转为垂直 baseline，
      // 并水平向左偏移半个大写字母高度。
      verticalInfo->extraMatrix.setRotate(90);
      auto offsetX = (metrics.capHeight + metrics.xHeight) * 0.25f;
      verticalInfo->extraMatrix.postTranslate(-offsetX, 0);
      verticalInfo->ascent += offsetX;
      verticalInfo->descent += offsetX;
    } else {
      auto offset = _font.getGlyphVerticalOffset(_glyphId);
      verticalInfo->extraMatrix.postTranslate(offset.x, offset.y);
      auto width = verticalInfo->advance;
      verticalInfo->advance = _font.getGlyphAdvance(_glyphId, true);
      if (verticalInfo->advance == 0) {
        verticalInfo->advance = width;
      }
      verticalInfo->ascent = -width * 0.5f;
      verticalInfo->descent = width * 0.5f;
    }
    verticalInfo->extraMatrix.mapRect(&verticalInfo->bounds);
    info = verticalInfo.get();
  }
}

std::shared_ptr<Glyph> Glyph::makeHorizontalGlyph() const {
  auto glyph = std::make_shared<Glyph>(*this);
  if (glyph->isVertical()) {
    glyph->_isVertical = false;
    glyph->info = glyph->horizontalInfo.get();
    glyph->verticalInfo = nullptr;
  }
  return glyph;
}

std::vector<std::shared_ptr<Glyph>> GetSimpleGlyphs(const TextDocument* textDocument,
                                                    bool applyDirection) {
  tgfx::Font textFont = {};
  textFont.setFauxBold(textDocument->fauxBold);
  textFont.setFauxItalic(textDocument->fauxItalic);
  textFont.setSize(textDocument->fontSize);
  auto typeface =
      FontManager::GetTypefaceWithoutFallback(textDocument->fontFamily, textDocument->fontStyle);
  bool hasTypeface = typeface != nullptr;
  std::unordered_map<std::string, std::shared_ptr<Glyph>> glyphMap;
  std::vector<std::shared_ptr<Glyph>> glyphList;
  const char* textStart = &(textDocument->text[0]);
  const char* textStop = textStart + textDocument->text.size();
  while (textStart < textStop) {
    auto oldPosition = textStart;
    tgfx::UTF::NextUTF8(&textStart, textStop);
    auto length = textStart - oldPosition;
    auto name = std::string(oldPosition, length);
    if (glyphMap.find(name) != glyphMap.end()) {
      glyphList.push_back(glyphMap[name]);
      continue;
    }
    tgfx::GlyphID glyphId = 0;
    if (hasTypeface) {
      glyphId = typeface->getGlyphID(name);
      if (glyphId != 0) {
        textFont.setTypeface(typeface);
      }
    }
    if (glyphId == 0) {
      auto fallbackTypeface = FontManager::GetFallbackTypeface(name, &glyphId);
      textFont.setTypeface(fallbackTypeface);
    }
    bool isVertical = applyDirection && textDocument->direction == TextDirection::Vertical;
    auto glyph = std::make_shared<Glyph>(glyphId, name, textFont, isVertical);
    glyphMap[name] = glyph;
    glyphList.push_back(glyph);
  }
  return glyphList;
}
}  // namespace pag
