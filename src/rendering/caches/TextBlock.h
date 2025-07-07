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

#pragma once

#include "pag/file.h"
#include "rendering/graphics/Glyph.h"

namespace pag {
class TextBlock {
 public:
  TextBlock(ID assetID, std::vector<std::vector<GlyphHandle>> lines, float maxScale,
            const tgfx::Rect* textBounds = nullptr);

  ID id() const {
    return _id;
  }

  ID assetID() const {
    return _assetID;
  }

  const tgfx::Rect& textBounds() const {
    return _textBounds;
  }

  std::vector<GlyphHandle> maskAtlasGlyphs(float scale) const;

  std::vector<GlyphHandle> colorAtlasGlyphs(float scale) const;

  float maxScale() const {
    return _maxScale;
  }

  std::vector<std::vector<GlyphHandle>> lines() const {
    return _lines;
  }

 private:
  ID _id = 0;
  ID _assetID = 0;
  std::vector<std::vector<GlyphHandle>> _lines;
  std::vector<GlyphHandle> _maskAtlasGlyphs;
  std::vector<GlyphHandle> _colorAtlasGlyphs;
  float _maxScale = 1.0f;
  tgfx::Rect _textBounds = tgfx::Rect::MakeEmpty();
};
}  // namespace pag
