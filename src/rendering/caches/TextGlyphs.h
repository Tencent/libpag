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

#include <unordered_map>
#include "pag/file.h"
#include "rendering/graphics/MutableGlyph.h"

namespace pag {
TextPaint CreateTextPaint(const TextDocument* textDocument);

class TextGlyphs {
 public:
  TextGlyphs(ID assetID, TextDocument* textDocument, float maxScale,
             std::vector<GlyphHandle>& layoutGlyphs);

  ID id() const {
    return _id;
  }

  ID assetID() const {
    return _assetID;
  }

  TextDocument* textDocument() const {
    return _textDocument;
  }

  const std::vector<GlyphHandle>& maskAtlasGlyphs() const {
    return _maskAtlasGlyphs;
  }

  const std::vector<GlyphHandle>& colorAtlasGlyphs() const {
    return _colorAtlasGlyphs;
  }

  float maxScale() const {
    return _maxScale;
  }

  std::vector<GlyphHandle> getGlyphs() const;

 private:
  ID _id = 0;
  ID _assetID = 0;
  TextDocument* _textDocument;
  std::vector<std::shared_ptr<Glyph>> simpleGlyphs;
  std::vector<GlyphHandle> _maskAtlasGlyphs;
  std::vector<GlyphHandle> _colorAtlasGlyphs;
  float _maxScale = 1.0f;
};
}  // namespace pag
