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

#include "LayoutContext.h"
#include "pagx/FontConfig.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/Typeface.h"

namespace pagx {

LayoutContext::LayoutContext(FontConfig* fontConfig) : fontConfig(fontConfig) {
}

std::shared_ptr<tgfx::Typeface> LayoutContext::findTypeface(const std::string& fontFamily,
                                                            const std::string& fontStyle) const {
  if (fontConfig != nullptr) {
    return fontConfig->findTypeface(fontFamily, fontStyle);
  }
  if (!fontFamily.empty()) {
    return tgfx::Typeface::MakeFromName(fontFamily, fontStyle);
  }
  return nullptr;
}

std::vector<tgfx::Font> LayoutContext::getFallbackFonts(
    float fontSize, bool fauxBold, bool fauxItalic, const tgfx::Typeface* primaryTypeface) const {
  std::vector<tgfx::Font> fonts = {};
  if (fontConfig == nullptr) {
    return fonts;
  }
  for (auto& holder : fontConfig->fallbackTypefaces) {
    auto typeface = holder.getTypeface();
    if (typeface == nullptr || typeface.get() == primaryTypeface) {
      continue;
    }
    tgfx::Font font = {};
    font.setTypeface(typeface);
    font.setSize(fontSize);
    font.setFauxBold(fauxBold);
    font.setFauxItalic(fauxItalic);
    fonts.push_back(font);
  }
  return fonts;
}

}  // namespace pagx
