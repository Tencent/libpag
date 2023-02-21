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

#include "CGTextBlob.h"

namespace tgfx {
std::shared_ptr<TextBlob> TextBlob::MakeFrom(const GlyphID glyphIDs[], const Point positions[],
                                             size_t glyphCount, const Font& font) {
  if (glyphCount == 0) {
    return nullptr;
  }
  auto textBlob = std::make_shared<CGTextBlob>();
  textBlob->glyphIDs = {glyphIDs, glyphIDs + glyphCount};
  textBlob->positions = {positions, positions + glyphCount};
  textBlob->font = font;
  return textBlob;
}

std::shared_ptr<ImageBuffer> CGTextBlob::getImage(float /*resolutionScale*/,
                                                  Matrix* /*matrix*/) const {
  // TODO(domrjchen): Added the implementation of generating multiple character images at once and
  //  completed GLCanvas.drawColorGlyphs().
  return nullptr;
}
}  // namespace tgfx
