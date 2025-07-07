/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 Tencent. All rights reserved.
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

#include <vector>
#include "tgfx/core/Typeface.h"

namespace pag {
/**
 * ShapedGlyph represents a shaped glyph.
 */
struct ShapedGlyph {
  ShapedGlyph(std::shared_ptr<tgfx::Typeface> typeface, tgfx::GlyphID glyphID, uint32_t stringIndex)
      : typeface(std::move(typeface)), glyphIDs({glyphID}), stringIndex(stringIndex) {
  }

  std::shared_ptr<tgfx::Typeface> typeface = nullptr;
  // There may be multiple glyph IDs for a single character in the case of ligatures.
  std::vector<tgfx::GlyphID> glyphIDs = {};
  uint32_t stringIndex = 0;
};
}  // namespace pag
