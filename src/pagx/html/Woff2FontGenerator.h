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
};

/**
 * Builds a WOFF2 font file from a PAGX Font node containing vector glyph outlines. Returns an
 * empty result if the font contains any bitmap glyphs or has no glyphs.
 */
Woff2FontResult BuildWoff2FromFont(const Font* font, const std::string& fontId);

}  // namespace pagx
