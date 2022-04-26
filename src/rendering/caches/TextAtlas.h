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

#pragma once

#include "TextGlyphs.h"
#include "pag/types.h"
#include "tgfx/core/BytesKey.h"

namespace pag {
class RenderCache;
class Atlas;

struct AtlasLocator {
  size_t textureIndex = 0;
  tgfx::Rect location = tgfx::Rect::MakeEmpty();
};

class TextAtlas {
 public:
  static std::unique_ptr<TextAtlas> Make(const TextGlyphs* textGlyphs, RenderCache* renderCache,
                                         float scale);

  ~TextAtlas();

  ID textGlyphsID() const {
    return _textGlyphsID;
  }

  bool getLocator(const tgfx::BytesKey& bytesKey, AtlasLocator* locator) const;

  std::shared_ptr<tgfx::Texture> getAtlasTexture(size_t textureIndex) const;

  float scaleFactor() const {
    return scale;
  }

  size_t memoryUsage() const;

 private:
  TextAtlas(ID textGlyphsID, Atlas* maskAtlas, Atlas* colorAtlas, float scale)
      : _textGlyphsID(textGlyphsID), maskAtlas(maskAtlas), colorAtlas(colorAtlas), scale(scale) {
  }

  ID _textGlyphsID = 0;
  Atlas* maskAtlas = nullptr;
  Atlas* colorAtlas = nullptr;
  float scale = 1.0f;
};
}  // namespace pag
