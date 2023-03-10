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
#include <unordered_map>
#include "rendering/utils/shaper/TextShaper.h"
#include "tgfx/core/UTF.h"

namespace pag {
std::vector<GlyphHandle> Glyph::BuildFromText(const std::string& text, const tgfx::Font& font,
                                              const TextPaint& paint, bool isVertical) {
  auto textFont = font;
  std::unordered_map<std::string, GlyphHandle> glyphMap;
  std::vector<GlyphHandle> glyphList;
  auto positionedGlyphs = TextShaper::Shape(text, font.getTypeface());
  auto count = positionedGlyphs.glyphCount();
  for (size_t i = 0; i < count; ++i) {
    auto index = positionedGlyphs.getStringIndex(i);
    auto length = (i + 1 == count ? text.length() : positionedGlyphs.getStringIndex(i + 1)) - index;
    auto name = text.substr(index, length);
    if (glyphMap.find(name) != glyphMap.end()) {
      glyphList.emplace_back(std::make_shared<Glyph>(*glyphMap[name]));
      continue;
    }
    textFont.setTypeface(positionedGlyphs.getTypeface(i));
    auto glyph = std::shared_ptr<Glyph>(
        new Glyph(positionedGlyphs.getGlyphID(i), name, textFont, isVertical, paint));
    glyphMap[name] = glyph;
    glyphList.emplace_back(glyph);
  }
  return glyphList;
}

Glyph::Glyph(tgfx::GlyphID glyphId, std::string name, tgfx::Font font, bool isVertical,
             const TextPaint& textPaint)
    : _glyphId(glyphId), _name(std::move(name)), _font(std::move(font)), _isVertical(isVertical) {
  horizontalInfo->advance = _font.getGlyphAdvance(_glyphId);
  horizontalInfo->originPosition.set(horizontalInfo->advance / 2, 0);
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
    verticalInfo->originPosition.set(0, verticalInfo->advance / 2);
    verticalInfo->extraMatrix.mapRect(&verticalInfo->bounds);
    info = verticalInfo.get();
  }
  textStyle = textPaint.style;
  strokeOverFill = textPaint.strokeOverFill;
  fillColor = textPaint.fillColor;
  strokeColor = textPaint.strokeColor;
  strokeWidth = textPaint.strokeWidth;
}

void Glyph::computeStyleKey(tgfx::BytesKey* styleKey) const {
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
  styleKey->write(getFont().getTypeface()->uniqueID());
}

bool Glyph::isVisible() const {
  return matrix.invertible() && alpha != 0.0f && !getBounds().isEmpty();
}

tgfx::Matrix Glyph::getTotalMatrix() const {
  auto m = getExtraMatrix();
  m.postConcat(tgfx::Matrix::MakeScale(scale));
  m.postTranslate(-info->originPosition.x * scale, -info->originPosition.y * scale);
  m.postConcat(matrix);
  m.postTranslate(info->originPosition.x * scale, info->originPosition.y * scale);
  return m;
}

void Glyph::computeAtlasKey(tgfx::BytesKey* bytesKey, TextStyle style) const {
  bytesKey->write(static_cast<uint32_t>(getFont().getTypeface()->hasColor()));
  bytesKey->write(static_cast<uint32_t>(getGlyphID()));
  bytesKey->write(static_cast<uint32_t>(style));
}

std::shared_ptr<Glyph> Glyph::makeHorizontalGlyph() const {
  auto glyph = std::make_shared<Glyph>(*this);
  if (_isVertical) {
    glyph->_isVertical = false;
    glyph->info = glyph->horizontalInfo.get();
    glyph->verticalInfo = nullptr;
  }
  glyph->matrix = tgfx::Matrix::I();
  return glyph;
}

std::shared_ptr<Glyph> Glyph::makeScaledGlyph(float s) const {
  auto scaledFont = _font.makeWithSize(_font.getSize() * s);
  TextPaint textPaint;
  textPaint.style = textStyle;
  textPaint.strokeOverFill = strokeOverFill;
  textPaint.fillColor = fillColor;
  textPaint.strokeColor = strokeColor;
  textPaint.strokeWidth = strokeWidth * s;
  return std::shared_ptr<Glyph>(new Glyph(_glyphId, _name, scaledFont, _isVertical, textPaint));
}
}  // namespace pag
