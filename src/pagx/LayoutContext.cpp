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
#include "FontConfigData.h"
#include "SystemFonts.h"
#include "tgfx/core/Typeface.h"

namespace pagx {

LayoutContext::LayoutContext(FontConfig* fontConfig) : fontConfig(fontConfig) {
}

std::shared_ptr<tgfx::Typeface> LayoutContext::findTypeface(const std::string& fontFamily,
                                                            const std::string& fontStyle) {
  if (fontConfig == nullptr) {
    if (!fontFamily.empty()) {
      return tgfx::Typeface::MakeFromName(fontFamily, fontStyle);
    }
    return nullptr;
  }

  // Stages 1-3 (registered exact, registered family, user fallback) live on FontConfig now so
  // the PAG v2 FontProvider adapter can reuse the exact same lookup. Stages 4-6 are layout-
  // context-specific (web behavior + system fallback) and stay here.
  if (auto typeface = fontConfig->findTypeface(fontFamily, fontStyle)) {
    return typeface;
  }

#ifdef PAG_BUILD_FOR_WEB
  // Stage 4: On web, prefer the first registered fallback typeface over browser fonts.
  if (!fontConfig->data->fallbackTypefaces.empty()) {
    return fontConfig->data->fallbackTypefaces.front().getTypeface();
  }
#endif

  // Stage 5: System font lookup via MakeFromName
  if (!fontFamily.empty()) {
    auto typeface = tgfx::Typeface::MakeFromName(fontFamily, fontStyle);
    if (typeface != nullptr) {
      return typeface;
    }
  }

  // Stage 6: Family-name match in system fallback typefaces
  if (!fontFamily.empty()) {
    ensureSystemFallbacks();
    for (auto& holder : systemFallbacks) {
      if (holder.getFontFamily() == fontFamily) {
        return holder.getTypeface();
      }
    }
  }

  return nullptr;
}

std::shared_ptr<tgfx::Typeface> LayoutContext::fallbackTypeface(
    int32_t codepoint, const tgfx::Typeface* primaryTypeface) {
  if (fontConfig != nullptr) {
    // Search user-registered fallback fonts first.
    for (auto& holder : fontConfig->data->fallbackTypefaces) {
      auto typeface = holder.getTypeface();
      if (typeface == nullptr || typeface.get() == primaryTypeface) {
        continue;
      }
      if (typeface->getGlyphID(codepoint) != 0) {
        return typeface;
      }
    }
  }
  // Search system fallback fonts.
  ensureSystemFallbacks();
  for (auto& holder : systemFallbacks) {
    auto typeface = holder.getTypeface();
    if (typeface == nullptr || typeface.get() == primaryTypeface) {
      continue;
    }
    if (typeface->getGlyphID(codepoint) != 0) {
      return typeface;
    }
  }
  return nullptr;
}

void LayoutContext::ensureSystemFallbacks() {
  if (systemFallbacksLoaded) {
    return;
  }
  systemFallbacksLoaded = true;
  auto locations = SystemFonts::FallbackTypefaces();
  for (auto& loc : locations) {
    systemFallbacks.emplace_back(loc.path, loc.ttcIndex, loc.fontFamily, loc.fontStyle);
  }
}

}  // namespace pagx
