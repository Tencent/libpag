/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#ifdef PAG_USE_HARFBUZZ

#include <string>
#include <vector>
#include "tgfx/core/Font.h"

namespace pagx {

struct ShapedGlyph {
  tgfx::GlyphID glyphID = 0;
  uint32_t cluster = 0;
  float xAdvance = 0;
  float yAdvance = 0;
  float xOffset = 0;
  float yOffset = 0;
  tgfx::Font font = {};
};

class HarfBuzzShaper {
 public:
  /**
   * Shapes the given UTF-8 text using the primary font and optional fallback fonts. Returns shaped
   * glyphs with positioning info. Glyphs with glyphID=0 could not be shaped by any font.
   */
  static std::vector<ShapedGlyph> Shape(const std::string& text, const tgfx::Font& primaryFont,
                                        const std::vector<tgfx::Font>& fallbackFonts = {},
                                        bool vertical = false, bool rtl = false);

  /**
   * Purges internal HarfBuzz font caches.
   */
  static void PurgeCaches();
};

}  // namespace pagx

#endif
