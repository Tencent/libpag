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

#include "TextBlock.h"
#include "base/utils/UniqueID.h"

namespace pag {
static void AddGlyph(const GlyphHandle& glyph, std::vector<GlyphHandle>* atlasGlyphs,
                     std::vector<tgfx::BytesKey>* atlasKeys) {
  if (glyph->getStyle() == TextStyle::Stroke && glyph->getStrokeWidth() < 0.1f) {
    return;
  }
  tgfx::BytesKey atlasKey;
  glyph->computeAtlasKey(&atlasKey, glyph->getStyle());
  if (std::find(atlasKeys->begin(), atlasKeys->end(), atlasKey) != atlasKeys->end()) {
    return;
  }
  atlasGlyphs->push_back(glyph);
  atlasKeys->push_back(atlasKey);
}

static void SortAtlasGlyphs(std::vector<GlyphHandle>* glyphs) {
  if (glyphs->empty()) {
    return;
  }
  std::sort(glyphs->begin(), glyphs->end(), [](const GlyphHandle& a, const GlyphHandle& b) -> bool {
    float aStrokeWidth = a->getStrokeWidth();
    float bStrokeWidth = b->getStrokeWidth();
    auto aWidth = a->getBounds().width() + aStrokeWidth * 2;
    auto aHeight = a->getBounds().height() + aStrokeWidth * 2;
    auto bWidth = b->getBounds().width() + bStrokeWidth * 2;
    auto bHeight = b->getBounds().height() + bStrokeWidth * 2;
    return aWidth * aHeight > bWidth * bHeight;
  });
}

TextBlock::TextBlock(ID assetID, std::vector<std::vector<GlyphHandle>> lines, float maxScale,
                     const tgfx::Rect* textBounds)
    : _id(UniqueID::Next()), _assetID(assetID), _lines(std::move(lines)), _maxScale(maxScale) {
  if (textBounds) {
    _textBounds = *textBounds;
  }
  std::vector<tgfx::BytesKey> atlasKeys;
  for (const auto& line : this->lines()) {
    for (const auto& tempGlyph : line) {
      auto glyph = tempGlyph->makeHorizontalGlyph();
      auto hasColor = glyph->getFont().getTypeface()->hasColor();
      auto style = glyph->getStyle();
      if (hasColor && (style == TextStyle::Stroke || style == TextStyle::StrokeAndFill)) {
        glyph->setStrokeWidth(0);
        glyph->setStyle(TextStyle::Fill);
      }
      auto& atlasGlyphs = hasColor ? _colorAtlasGlyphs : _maskAtlasGlyphs;
      if (style == TextStyle::Stroke || style == TextStyle::Fill) {
        AddGlyph(glyph, &atlasGlyphs, &atlasKeys);
      } else if (style == TextStyle::StrokeAndFill) {
        auto strokeGlyph = std::make_shared<Glyph>(*glyph);
        strokeGlyph->setStyle(TextStyle::Stroke);
        AddGlyph(strokeGlyph, &atlasGlyphs, &atlasKeys);
        glyph->setStyle(TextStyle::Fill);
        glyph->setStrokeWidth(0);
        AddGlyph(glyph, &atlasGlyphs, &atlasKeys);
      }
    }
  }
  SortAtlasGlyphs(&_maskAtlasGlyphs);
  SortAtlasGlyphs(&_colorAtlasGlyphs);
}
}  // namespace pag
