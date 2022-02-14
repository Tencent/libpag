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
#include "base/utils/UTF8Text.h"
#include "rendering/FontManager.h"

namespace pag {
std::vector<GlyphHandle> Glyph::BuildFromText(const std::string& text, const TextPaint& textPaint) {
  Font textFont = {};
  textFont.setFauxBold(textPaint.fauxBold);
  textFont.setFauxItalic(textPaint.fauxItalic);
  textFont.setSize(textPaint.fontSize);
  auto typeface =
      FontManager::GetTypefaceWithoutFallback(textPaint.fontFamily, textPaint.fontStyle);
  bool hasTypeface = typeface != nullptr;
  std::vector<GlyphHandle> glyphList;
  const char* textStart = &(text[0]);
  const char* textStop = textStart + text.size();
  while (textStart < textStop) {
    auto oldPosition = textStart;
    UTF8Text::NextChar(&textStart);
    auto length = textStart - oldPosition;
    auto name = std::string(oldPosition, length);
    GlyphID glyphId = 0;
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
    auto glyph = std::shared_ptr<Glyph>(new Glyph(glyphId, name, textFont, textPaint));
    glyphList.push_back(glyph);
  }
  return glyphList;
}

Glyph::Glyph(GlyphID glyphId, const std::string& charName, const Font& textFont,
             const TextPaint& textPaint)
    : glyphId(glyphId), textFont(textFont) {
  name = charName;
  _isVertical = textPaint.isVertical;
  textStyle = textPaint.style;
  strokeOverFill = textPaint.strokeOverFill;
  fillColor = textPaint.fillColor;
  strokeColor = textPaint.strokeColor;
  strokeWidth = textPaint.strokeWidth;
  auto metrics = textFont.getMetrics();
  ascent = metrics.ascent;
  descent = metrics.descent;
  advance = textFont.getGlyphAdvance(glyphId);
  bounds = textFont.getGlyphBounds(glyphId);
  if (charName == " ") {
    // 空格字符测量的 bounds 比较异常偏上，本身也不可见，这里直接按字幕 A 的上下边界调整一下。
    auto AGlyphID = textFont.getGlyphID("A");
    if (AGlyphID > 0) {
      auto ABounds = textFont.getGlyphBounds(AGlyphID);
      bounds.top = ABounds.top;
      bounds.bottom = ABounds.bottom;
    }
  }
  if (textPaint.isVertical) {
    if (name.size() == 1) {
      // 字母，数字，标点等字符旋转 90° 绘制，原先的水平 baseline 转为垂直 baseline，
      // 并水平向左偏移半个大写字母高度。
      extraMatrix.setRotate(90);
      auto offsetX = (metrics.capHeight + metrics.xHeight) * 0.25f;
      extraMatrix.postTranslate(-offsetX, 0);
      ascent += offsetX;
      descent += offsetX;
    } else {
      auto offset = textFont.getGlyphVerticalOffset(glyphId);
      extraMatrix.postTranslate(offset.x, offset.y);
      auto width = advance;
      advance = textFont.getGlyphAdvance(glyphId, true);
      ascent = -width * 0.5f;
      descent = width * 0.5f;
    }
    extraMatrix.mapRect(&bounds);
  }
}

void Glyph::computeStyleKey(BytesKey* styleKey) const {
  auto m = getTotalMatrix();
  styleKey->write(m.getScaleX());
  styleKey->write(m.getSkewX());
  styleKey->write(m.getSkewY());
  styleKey->write(m.getScaleY());
  uint8_t fillValues[] = {fillColor.red, fillColor.green, fillColor.blue,
                          static_cast<uint8_t>(alpha * 255)};
  styleKey->write(fillValues);
  uint8_t strokeValues[] = {strokeColor.red, strokeColor.green, strokeColor.blue,
                            static_cast<uint8_t>(textStyle)};
  styleKey->write(strokeValues);
  styleKey->write(strokeWidth);
  styleKey->write(textFont.getTypeface()->uniqueID());
}

bool Glyph::isVisible() const {
  return matrix.invertible() && alpha != 0.0f && !bounds.isEmpty();
}

Matrix Glyph::getTotalMatrix() const {
  auto m = extraMatrix;
  m.postConcat(matrix);
  return m;
}

}  // namespace pag