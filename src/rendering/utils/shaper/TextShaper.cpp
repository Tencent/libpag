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

#include "TextShaper.h"
#ifdef PAG_USE_HARFBUZZ
#include "TextShaperHarfbuzz.h"
#else
#include "TextShaperPrimitive.h"
#include "platform/Platform.h"
#endif

namespace pag {
std::vector<ShapedGlyph> TextShaper::Shape(const std::string& text,
                                           std::shared_ptr<tgfx::Typeface> typeface) {
  if (text.empty()) {
    return {};
  }
#ifdef PAG_USE_HARFBUZZ
  return TextShaperHarfbuzz::Shape(text, std::move(typeface));
#else
#ifndef TGFX_USE_FREETYPE
  auto glyphs = Platform::Current()->shapeText(text, typeface);
  if (!glyphs.empty()) {
    return glyphs;
  }
#endif
  return TextShaperPrimitive::Shape(text, std::move(typeface));
#endif
}

void TextShaper::PurgeCaches() {
#ifdef PAG_USE_HARFBUZZ
  TextShaperHarfbuzz::PurgeCaches();
#endif
}
}  // namespace pag
