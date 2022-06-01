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

#include "pag/types.h"
#include "tgfx/core/Font.h"

namespace pag {
class Glyph {
 public:
  Glyph(tgfx::GlyphID glyphId, std::string name, tgfx::Font font, bool isVertical);

  tgfx::GlyphID getGlyphID() const {
    return _glyphId;
  }

  std::string getName() const {
    return _name;
  }

  const tgfx::Font& getFont() const {
    return _font;
  }

  bool isVertical() const {
    return _isVertical;
  }

  float advance() const {
    return info->advance;
  }

  float ascent() const {
    return info->ascent;
  }

  float descent() const {
    return info->descent;
  }

  const tgfx::Rect& getBounds() const {
    return info->bounds;
  }

  const tgfx::Matrix& extraMatrix() const {
    return info->extraMatrix;
  }

  std::shared_ptr<Glyph> makeHorizontalGlyph() const;

 private:
  struct Info {
    float advance = 0;
    float ascent = 0;
    float descent = 0;
    tgfx::Rect bounds = tgfx::Rect::MakeEmpty();
    tgfx::Matrix extraMatrix = tgfx::Matrix::I();
  };

  tgfx::GlyphID _glyphId = 0;
  std::string _name;
  tgfx::Font _font;
  bool _isVertical = false;
  std::shared_ptr<Info> horizontalInfo = std::make_shared<Info>();
  std::shared_ptr<Info> verticalInfo;
  const Info* info = horizontalInfo.get();
};

std::vector<std::shared_ptr<Glyph>> GetSimpleGlyphs(const TextDocument* textDocument,
                                                    bool applyDirection = true);
}  // namespace pag
