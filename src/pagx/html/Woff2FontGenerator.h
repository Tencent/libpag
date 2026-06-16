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

#include <cstdint>
#include <string>
#include <vector>

namespace pagx {

class Font;

struct Woff2FontResult {
  std::vector<uint8_t> woff2Data;
  std::string familyName;
  std::string relativeUrl;
  // Exported OpenType unitsPerEm read back from the generated SFNT head table.
  uint16_t unitsPerEm = 0;
  // Scale applied to PAGX font design values before writing them into the generated font.
  float designScale = 1.0f;
  // Smallest non-empty vector glyph advance written to the generated hmtx table.
  uint16_t minVisibleGlyphAdvance = 0;
};

/**
 * Builds a WOFF2 font file from a PAGX Font node containing vector outlines or bitmap glyphs.
 * Returns an empty result if the font has no glyphs or contains incompatible glyph data.
 */
Woff2FontResult BuildWoff2FromFont(const Font* font, const std::string& fontId);

}  // namespace pagx
