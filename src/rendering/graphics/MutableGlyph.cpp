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

#include "MutableGlyph.h"

namespace pag {
std::vector<GlyphHandle> MutableGlyph::BuildFromText(
    const std::vector<std::shared_ptr<Glyph>>& glyphs, const TextPaint& paint) {
  std::vector<GlyphHandle> glyphList;
  glyphList.reserve(glyphs.size());
  for (const auto& glyph : glyphs) {
    glyphList.emplace_back(std::shared_ptr<MutableGlyph>(new MutableGlyph(glyph, paint)));
  }
  return glyphList;
}

GlyphHandle MutableGlyph::Make(const std::shared_ptr<Glyph> glyph, const TextPaint& paint) {
  return std::shared_ptr<MutableGlyph>(new MutableGlyph(glyph, paint));
}

MutableGlyph::MutableGlyph(std::shared_ptr<Glyph> simpleGlyph, const TextPaint& textPaint)
    : simpleGlyph(std::move(simpleGlyph)) {
  textStyle = textPaint.style;
  strokeOverFill = textPaint.strokeOverFill;
  fillColor = textPaint.fillColor;
  strokeColor = textPaint.strokeColor;
  strokeWidth = textPaint.strokeWidth;
}

void MutableGlyph::computeStyleKey(tgfx::BytesKey* styleKey) const {
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

bool MutableGlyph::isVisible() const {
  return matrix.invertible() && alpha != 0.0f && !getBounds().isEmpty();
}

tgfx::Matrix MutableGlyph::getTotalMatrix() const {
  auto m = getExtraMatrix();
  m.postConcat(matrix);
  return m;
}

void MutableGlyph::computeAtlasKey(tgfx::BytesKey* bytesKey, TextStyle style) const {
  bytesKey->write(static_cast<uint32_t>(getFont().getTypeface()->hasColor()));
  bytesKey->write(static_cast<uint32_t>(getGlyphID()));
  bytesKey->write(static_cast<uint32_t>(style));
}
}  // namespace pag
