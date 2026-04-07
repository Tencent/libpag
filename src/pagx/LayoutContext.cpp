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

static int StylePriority(const std::string& style) {
  if (style == "Regular") {
    return 0;
  }
  if (style == "Medium") {
    return 1;
  }
  if (style == "Normal") {
    return 2;
  }
  return 3;
}

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

  // Stage 1: Exact match in registered typefaces
  if (!fontFamily.empty()) {
    FontConfig::Data::FontKey key = {};
    key.family = fontFamily;
    key.style = fontStyle.empty() ? "Regular" : fontStyle;
    auto it = fontConfig->data->registeredTypefaces.find(key);
    if (it != fontConfig->data->registeredTypefaces.end()) {
      return it->second;
    }

    // Stage 2: Family-name match in registered typefaces (prefer Regular style)
    std::shared_ptr<tgfx::Typeface> bestTypeface = nullptr;
    int bestPriority = 4;
    std::string bestStyle = {};
    for (const auto& pair : fontConfig->data->registeredTypefaces) {
      if (pair.first.family != fontFamily) {
        continue;
      }
      int priority = StylePriority(pair.first.style);
      bool preferred = (bestTypeface == nullptr) || (priority < bestPriority) ||
                       (priority == bestPriority && pair.first.style < bestStyle);
      if (preferred) {
        bestTypeface = pair.second;
        bestPriority = priority;
        bestStyle = pair.first.style;
      }
    }
    if (bestTypeface != nullptr) {
      return bestTypeface;
    }
  }

  // Stage 3: Family-name match in user fallback typefaces
  if (!fontFamily.empty()) {
    for (auto& holder : fontConfig->data->fallbackTypefaces) {
      if (holder.getFontFamily() == fontFamily) {
        return holder.getTypeface();
      }
    }
  }

  // Stage 4: System font lookup via MakeFromName
  if (!fontFamily.empty()) {
    auto typeface = tgfx::Typeface::MakeFromName(fontFamily, fontStyle);
    if (typeface != nullptr) {
      return typeface;
    }
  }

  // Stage 5: Family-name match in system fallback typefaces
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
