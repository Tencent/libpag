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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "LayoutContext.h"
#include "FontConfigData.h"
#include "SystemFonts.h"
#include "renderer/TextUnicode.h"
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

static std::shared_ptr<tgfx::Typeface> ResolveLayoutTypeface(const std::string& fontFamily,
                                                             const std::string& fontStyle,
                                                             float size) {
#ifdef __APPLE__
  return SystemFonts::ResolveTypeface(fontFamily, fontStyle, size);
#else
  auto namedTypeface = tgfx::Typeface::MakeFromName(fontFamily, fontStyle);
  auto exactLocation = SystemFonts::FindFont(fontFamily, fontStyle);
  // The platform reports the style under its own spelling, which may differ from the requested one
  // in case or spacing only, so compare the two the way every other font-name decision here does.
  bool exactStyleExists =
      namedTypeface != nullptr && !exactLocation.path.empty() &&
      (fontStyle.empty() || SystemFonts::FontNamesMatch(fontStyle, exactLocation.fontStyle));
  if (exactStyleExists) {
    // Preserve the platform's existing exact-style behavior. Some FreeType configurations cannot
    // load an installed exact face by name, and changing that behavior affects established layout
    // metrics. The same-family fallback below is only needed when the requested style is absent.
    return namedTypeface;
  }
  return SystemFonts::ResolveTypeface(fontFamily, fontStyle, size);
#endif
}

// Memoised `ResolveLayoutTypeface`: the platform lookup walks the installed font list, and a
// document repeats the same (family, style) pair for every text node that uses it. The two halves
// are joined with a separator byte that no family name contains, so distinct pairs cannot collide.
// A failed lookup is cached too — the platform answer does not change within one document.
static std::shared_ptr<tgfx::Typeface> ResolveCachedLayoutTypeface(
    std::unordered_map<std::string, std::shared_ptr<tgfx::Typeface>>& cache,
    const std::string& fontFamily, const std::string& fontStyle, float size = 12.0f) {
  auto cacheKey = fontFamily + "\x1f" + fontStyle + "\x1f" + std::to_string(size);
  auto it = cache.find(cacheKey);
  if (it == cache.end()) {
    it = cache.emplace(cacheKey, ResolveLayoutTypeface(fontFamily, fontStyle, size)).first;
  }
  return it->second;
}

LayoutContext::LayoutContext(FontConfig* fontConfig) : fontConfig(fontConfig) {
}

std::shared_ptr<tgfx::Typeface> LayoutContext::findTypeface(const std::string& fontFamily,
                                                            const std::string& fontStyle,
                                                            int32_t codepoint, float size) {
  if (fontConfig == nullptr) {
    if (!fontFamily.empty()) {
      return ResolveCachedLayoutTypeface(systemTypefaceCache, fontFamily, fontStyle, size);
    }
    return nullptr;
  }

  auto faces = fontConfig->data->fontFaces.find(fontFamily);
  if (faces != fontConfig->data->fontFaces.end()) {
    for (auto& face : faces->second) {
      bool covered = codepoint < 0 || face.unicodeRanges.empty();
      for (const auto& range : face.unicodeRanges) {
        covered |= codepoint >= 0 && static_cast<uint32_t>(codepoint) >= range.first &&
                   static_cast<uint32_t>(codepoint) <= range.second;
      }
      if (!covered) continue;
      auto typeface = face.holder.getTypeface(size);
      if (typeface && (codepoint < 0 || typeface->getGlyphID(codepoint) != 0)) return typeface;
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
      return it->second.getTypeface(size);
    }

    // Stage 2: Family-name match in registered typefaces (prefer Regular style)
    std::shared_ptr<tgfx::Typeface> bestTypeface = nullptr;
    int bestPriority = 4;
    std::string bestStyle = {};
    for (auto& pair : fontConfig->data->registeredTypefaces) {
      if (pair.first.family != fontFamily) {
        continue;
      }
      int priority = StylePriority(pair.first.style);
      bool preferred = (bestTypeface == nullptr) || (priority < bestPriority) ||
                       (priority == bestPriority && pair.first.style < bestStyle);
      if (preferred) {
        bestTypeface = pair.second.getTypeface(size);
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
        return holder.getTypeface(size);
      }
    }
  }

#ifdef PAG_BUILD_FOR_WEB
  // Stage 4: On web, prefer the first registered fallback typeface over browser fonts.
  if (!fontConfig->data->fallbackTypefaces.empty()) {
    return fontConfig->data->fallbackTypefaces.front().getTypeface(size);
  }
#endif

  // Stage 5: System font lookup, including another available style when the requested one is
  // absent from the family.
  if (!fontFamily.empty()) {
    auto typeface = ResolveCachedLayoutTypeface(systemTypefaceCache, fontFamily, fontStyle, size);
    if (typeface != nullptr) {
      return typeface;
    }
  }

  // Stage 6: Family-name match in system fallback typefaces
  if (!fontFamily.empty()) {
    ensureSystemFallbacks();
    for (auto& holder : systemFallbacks) {
      if (holder.getFontFamily() == fontFamily) {
        return holder.getTypeface(size);
      }
    }
  }

  return nullptr;
}

std::shared_ptr<tgfx::Typeface> LayoutContext::fallbackTypeface(
    int32_t codepoint, const tgfx::Typeface* primaryTypeface) {
  return fallbackTypeface(
      [&](const auto& typeface) {
        return typeface.get() != primaryTypeface && typeface->getGlyphID(codepoint) != 0;
      },
      12.0f);
}

std::shared_ptr<tgfx::Typeface> LayoutContext::findClusterTypeface(
    const std::string& family, const std::string& style, float size,
    const std::vector<int32_t>& codepoints, const TypefacePredicate& accepts) {
  if (fontConfig) {
    auto faces = fontConfig->data->fontFaces.find(family);
    if (faces != fontConfig->data->fontFaces.end()) {
      for (auto& face : faces->second) {
        bool covered = true;
        for (auto codepoint : codepoints) {
          if (IsDefaultIgnorable(codepoint) || face.unicodeRanges.empty()) continue;
          bool inRange = false;
          for (const auto& range : face.unicodeRanges) {
            inRange |= static_cast<uint32_t>(codepoint) >= range.first &&
                       static_cast<uint32_t>(codepoint) <= range.second;
          }
          covered &= inRange;
        }
        if (!covered) continue;
        auto typeface = face.holder.getTypeface(size);
        if (typeface && accepts(typeface)) return typeface;
      }
      return nullptr;
    }
  }
  auto typeface = findTypeface(family, style, -1, size);
  return typeface && accepts(typeface) ? typeface : nullptr;
}

std::shared_ptr<tgfx::Typeface> LayoutContext::fallbackTypeface(const TypefacePredicate& accepts,
                                                                float size) {
  if (fontConfig != nullptr) {
    // Search user-registered fallback fonts first.
    for (auto& holder : fontConfig->data->fallbackTypefaces) {
      auto typeface = holder.getTypeface(size);
      if (typeface == nullptr) {
        continue;
      }
      if (accepts(typeface)) {
        return typeface;
      }
    }
  }
  // Search system fallback fonts.
  ensureSystemFallbacks();
  for (auto& holder : systemFallbacks) {
    auto typeface = ResolveCachedLayoutTypeface(systemTypefaceCache, holder.getFontFamily(),
                                                holder.getFontStyle(), size);
    if (typeface == nullptr) typeface = holder.getTypeface(size);
    if (typeface == nullptr) {
      continue;
    }
    if (accepts(typeface)) {
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
