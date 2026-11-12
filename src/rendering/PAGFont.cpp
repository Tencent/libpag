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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <mutex>
#include "pag/pag.h"
#include "pagx/FontConfig.h"
#include "pagx/GlobalFontRegistry.h"
#include "rendering/FontManager.h"

namespace pag {
// Every entry point mirrors the registration into the PAGX global registry (design: the new
// runtime is fed by the same global font interfaces), so hosts register fonts once and both the
// legacy renderer and PAGX documents resolve them. All FontManager mutations flow through this
// class, so the mirror stays complete.
PAGFont PAGFont::RegisterFont(const std::string& fontPath, int ttcIndex,
                              const std::string& fontFamily, const std::string& fontStyle) {
  auto font = FontManager::RegisterFont(fontPath, ttcIndex, fontFamily, fontStyle);
  if (!font.fontFamily.empty()) {
    std::lock_guard<std::mutex> globalLock(pagx::GlobalFontRegistry::GetMutex());
    pagx::GlobalFontRegistry::GetFontConfig().registerFont(
        pagx::PAGFont(font.fontFamily, font.fontStyle), fontPath, ttcIndex);
  }
  return font;
}

PAGFont PAGFont::RegisterFont(const void* data, size_t length, int ttcIndex,
                              const std::string& fontFamily, const std::string& fontStyle) {
  auto font = FontManager::RegisterFont(data, length, ttcIndex, fontFamily, fontStyle);
  if (!font.fontFamily.empty()) {
    std::lock_guard<std::mutex> globalLock(pagx::GlobalFontRegistry::GetMutex());
    pagx::GlobalFontRegistry::GetFontConfig().registerFont(
        pagx::PAGFont(font.fontFamily, font.fontStyle), data, length, ttcIndex);
  }
  return font;
}

void PAGFont::UnregisterFont(const PAGFont& font) {
  FontManager::UnregisterFont(font);
  if (!font.fontFamily.empty()) {
    std::lock_guard<std::mutex> globalLock(pagx::GlobalFontRegistry::GetMutex());
    pagx::GlobalFontRegistry::GetFontConfig().unregisterFont(
        pagx::PAGFont(font.fontFamily, font.fontStyle));
  }
}

void PAGFont::SetFallbackFontNames(const std::vector<std::string>& fontNames) {
  FontManager::SetFallbackFontNames(fontNames);
  pagx::GlobalFontRegistry::SetFallbackFontNames(fontNames);
}

void PAGFont::SetFallbackFontPaths(const std::vector<std::string>& fontPaths,
                                   const std::vector<int>& ttcIndices) {
  FontManager::SetFallbackFontPaths(fontPaths, ttcIndices);
  std::lock_guard<std::mutex> globalLock(pagx::GlobalFontRegistry::GetMutex());
  auto& config = pagx::GlobalFontRegistry::GetFontConfig();
  config.clearFallbackFonts();
  for (size_t i = 0; i < fontPaths.size(); i++) {
    config.addFallbackFont(fontPaths[i], i < ttcIndices.size() ? ttcIndices[i] : 0, "", "");
  }
}
}  // namespace pag
