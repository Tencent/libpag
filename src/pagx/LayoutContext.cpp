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
#include <cstdlib>
#include "FontConfigData.h"
#include "SystemFonts.h"
#include "pagx/utils/CSSFontStyle.h"
#include "tgfx/core/Typeface.h"

namespace pagx {

static int StyleMatchDistance(const ParsedFontStyle& candidate, const ParsedFontStyle& requested) {
  // Slant mismatches are penalised heavily so that the closest-weight upright never wins
  // over an italic match (and vice versa) when the caller asked for one. The 1000-step
  // penalty exceeds the maximum possible weight gap (900 - 100 = 800).
  int weightDiff = std::abs(candidate.weight - requested.weight);
  int slantPenalty = (candidate.italic != requested.italic) ? 1000 : 0;
  return weightDiff + slantPenalty;
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

    // Stage 2: Family-name match in registered typefaces. Pick the candidate whose parsed
    // weight + slant is closest to the request so that asking for `Black Italic` prefers a
    // registered Black face over Regular when both are present. Lexicographic style ordering
    // is used as a stable tiebreak when two faces sit at equal distance.
    auto requested = ParseFontStyleName(fontStyle);
    std::shared_ptr<tgfx::Typeface> bestTypeface = nullptr;
    int bestDistance = 0;
    std::string bestStyle = {};
    for (const auto& pair : fontConfig->data->registeredTypefaces) {
      if (pair.first.family != fontFamily) {
        continue;
      }
      auto candidate = ParseFontStyleName(pair.first.style);
      int distance = StyleMatchDistance(candidate, requested);
      bool preferred = (bestTypeface == nullptr) || (distance < bestDistance) ||
                       (distance == bestDistance && pair.first.style < bestStyle);
      if (preferred) {
        bestTypeface = pair.second;
        bestDistance = distance;
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
