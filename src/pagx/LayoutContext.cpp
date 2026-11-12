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
#include "GlobalFontRegistry.h"
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

// Stages 1-3 of findTypeface for any FontConfig: exact (family, style) key, then family match
// among registered typefaces preferring Regular, then family match among fallback typefaces.
std::shared_ptr<tgfx::Typeface> LayoutContext::FindInFontConfig(FontConfig* config,
                                                                const std::string& fontFamily,
                                                                const std::string& fontStyle) {
  if (fontFamily.empty()) {
    return nullptr;
  }
  FontConfig::Data::FontKey key = {};
  key.family = fontFamily;
  key.style = fontStyle.empty() ? "Regular" : fontStyle;
  auto it = config->data->registeredTypefaces.find(key);
  if (it != config->data->registeredTypefaces.end()) {
    return it->second.getTypeface();
  }
  std::shared_ptr<tgfx::Typeface> bestTypeface = nullptr;
  int bestPriority = 4;
  std::string bestStyle = {};
  for (auto& pair : config->data->registeredTypefaces) {
    if (pair.first.family != fontFamily) {
      continue;
    }
    int priority = StylePriority(pair.first.style);
    bool preferred = (bestTypeface == nullptr) || (priority < bestPriority) ||
                     (priority == bestPriority && pair.first.style < bestStyle);
    if (preferred) {
      bestTypeface = pair.second.getTypeface();
      bestPriority = priority;
      bestStyle = pair.first.style;
    }
  }
  if (bestTypeface != nullptr) {
    return bestTypeface;
  }
  for (auto& holder : config->data->fallbackTypefaces) {
    if (holder.getFontFamily() == fontFamily) {
      return holder.getTypeface();
    }
  }
  return nullptr;
}

std::shared_ptr<tgfx::Typeface> LayoutContext::findTypeface(const std::string& fontFamily,
                                                            const std::string& fontStyle) {
  // The process-wide registry (PAGFont::RegisterFont / SetFallbackFontNames) mirrors the legacy
  // global font API, so hosts only need the global calls to feed every document.
  if (fontConfig == nullptr) {
    if (!fontFamily.empty()) {
      std::lock_guard<std::mutex> globalLock(GlobalFontRegistry::GetMutex());
      return FindInFontConfig(&GlobalFontRegistry::GetFontConfig(), fontFamily, fontStyle);
    }
    return nullptr;
  }

  // Stages 1-3: the document's own FontConfig takes precedence.
  auto typeface = FindInFontConfig(fontConfig, fontFamily, fontStyle);
  if (typeface != nullptr) {
    return typeface;
  }

  {
    std::lock_guard<std::mutex> globalLock(GlobalFontRegistry::GetMutex());
    auto global = FindInFontConfig(&GlobalFontRegistry::GetFontConfig(), fontFamily, fontStyle);
    if (global != nullptr) {
      return global;
    }
  }

#ifdef PAG_BUILD_FOR_WEB
  // Stage 4: On web, prefer the first registered fallback typeface over browser fonts.
  if (!fontConfig->data->fallbackTypefaces.empty()) {
    return fontConfig->data->fallbackTypefaces.front().getTypeface();
  }
  {
    std::lock_guard<std::mutex> globalLock(GlobalFontRegistry::GetMutex());
    auto& globalFallbacks = GlobalFontRegistry::GetFontConfig().data->fallbackTypefaces;
    if (!globalFallbacks.empty()) {
      return globalFallbacks.front().getTypeface();
    }
  }
#endif

  // Unregistered families resolve to null on purpose: the legacy runtime never substitutes a
  // system font by name either, and a substituted face would silently change the layout metrics
  // of migrated documents. The shaper's per-character fallback covers the text with the fallback
  // fonts instead, exactly like the legacy TextShaper does.
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
  // Search the process-wide fallback fonts (PAGFont::SetFallbackFontNames) next.
  {
    std::lock_guard<std::mutex> globalLock(GlobalFontRegistry::GetMutex());
    for (auto& holder : GlobalFontRegistry::GetFontConfig().data->fallbackTypefaces) {
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
