/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "pag/c/pag_font.h"
#include "pag_types_priv.h"

pag_font* pag_font_register_from_path(const char* filePath, int ttcIndex, const char* fontFamily,
                                      const char* fontStyle) {
  std::string path(filePath);
  std::string family(fontFamily);
  std::string style(fontStyle);
  return new pag_font(pag::PAGFont::RegisterFont(path, ttcIndex, family, style));
}

pag_font* pag_font_register_from_data(const void* data, size_t length, int ttcIndex,
                                      const char* fontFamily, const char* fontStyle) {
  std::string family(fontFamily);
  std::string style(fontStyle);
  return new pag_font(pag::PAGFont::RegisterFont(data, length, ttcIndex, family, style));
}

const char* pag_font_get_family(pag_font* font) {
  if (font == nullptr || font->p.fontFamily.empty()) {
    return nullptr;
  }
  return font->p.fontFamily.c_str();
}

const char* pag_font_get_style(pag_font* font) {
  if (font == nullptr || font->p.fontStyle.empty()) {
    return nullptr;
  }
  return font->p.fontStyle.c_str();
}

void PAGFontRelease(pag_font* font) {
  delete font;
}
