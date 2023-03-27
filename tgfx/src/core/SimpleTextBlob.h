/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "tgfx/core/TextBlob.h"

namespace tgfx {
class SimpleTextBlob : public TextBlob {
 public:
  bool hasColor() const override;

  Rect getBounds(const Stroke* stroke = nullptr) const override;

  bool getPath(Path* path, const Stroke* stroke = nullptr) const override;

  const Font& getFont() const {
    return font;
  }

  const std::vector<GlyphID>& getGlyphIDs() const {
    return glyphIDs;
  }

  const std::vector<Point>& getPositions() const {
    return positions;
  }

 private:
  Font font = {};
  std::vector<GlyphID> glyphIDs = {};
  std::vector<Point> positions = {};

  friend class TextBlob;
};
}  // namespace tgfx
