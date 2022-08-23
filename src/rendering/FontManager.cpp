/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "FontManager.h"
#include "base/utils/USE.h"
#include "pag/file.h"
#include "platform/Platform.h"

namespace pag {
std::shared_ptr<TypefaceHolder> TypefaceHolder::MakeFromName(const std::string& fontFamily,
                                                             const std::string& fontStyle) {
  auto holder = new TypefaceHolder();
  holder->fontFamily = fontFamily;
  holder->fontStyle = fontStyle;
  return std::shared_ptr<TypefaceHolder>(holder);
}

std::shared_ptr<TypefaceHolder> TypefaceHolder::MakeFromFile(const std::string& fontPath,
                                                             int ttcIndex) {
  auto holder = new TypefaceHolder();
  holder->fontPath = fontPath;
  holder->ttcIndex = ttcIndex;
  return std::shared_ptr<TypefaceHolder>(holder);
}

std::shared_ptr<tgfx::Typeface> TypefaceHolder::getTypeface() {
  if (typeface == nullptr) {
    if (!fontPath.empty()) {
      typeface = tgfx::Typeface::MakeFromPath(fontPath, ttcIndex);
    } else {
      typeface = tgfx::Typeface::MakeFromName(fontFamily, fontStyle);
    }
  }
  return typeface;
}

FontManager::~FontManager() {
  std::lock_guard<std::mutex> autoLock(locker);
  registeredFontMap.clear();
}

bool FontManager::hasFallbackFonts() {
  std::lock_guard<std::mutex> autoLock(locker);
  return !fallbackFontList.empty();
}

PAGFont FontManager::registerFont(const std::string& fontPath, int ttcIndex,
                                  const std::string& fontFamily, const std::string& fontStyle) {
  std::lock_guard<std::mutex> autoLock(locker);
  auto typeface = tgfx::Typeface::MakeFromPath(fontPath, ttcIndex);
  return registerFont(typeface, fontFamily, fontStyle);
}

PAGFont FontManager::registerFont(const void* data, size_t length, int ttcIndex,
                                  const std::string& fontFamily, const std::string& fontStyle) {
  std::lock_guard<std::mutex> autoLock(locker);
  auto typeface = tgfx::Typeface::MakeFromBytes(data, length, ttcIndex);
  return registerFont(typeface, fontFamily, fontStyle);
}

static std::string PAGFontRegisterKey(const std::string& fontFamily, const std::string& fontStyle) {
  return fontFamily + "|" + fontStyle;
}

PAGFont FontManager::registerFont(std::shared_ptr<tgfx::Typeface> typeface,
                                  const std::string& fontFamily, const std::string& fontStyle) {
  if (typeface == nullptr) {
    return {"", ""};
  }
  std::string family = typeface->fontFamily();
  std::string style = typeface->fontStyle();
  if (!fontFamily.empty()) {
    family = fontFamily;
    style = fontStyle;
  }
  auto key = PAGFontRegisterKey(family, style);
  auto iter = registeredFontMap.find(key);
  if (iter != registeredFontMap.end()) {
    registeredFontMap.erase(iter);
  }
  registeredFontMap[key] = std::move(typeface);
  return {family, style};
}

void FontManager::unregisterFont(const PAGFont& font) {
  if (font.fontFamily.empty()) {
    return;
  }
  std::lock_guard<std::mutex> autoLock(locker);
  auto iter = registeredFontMap.find(PAGFontRegisterKey(font.fontFamily, font.fontStyle));
  if (iter == registeredFontMap.end()) {
    return;
  }
  registeredFontMap.erase(iter);
}

static std::shared_ptr<tgfx::Typeface> MakeTypefaceWithName(const std::string& fontFamily,
                                                            const std::string& fontStyle) {
  auto typeface = tgfx::Typeface::MakeFromName(fontFamily, fontStyle);
  if (typeface != nullptr) {
    if (fontFamily != typeface->fontFamily()) {
      typeface = nullptr;
    }
  }
  return typeface;
}

std::shared_ptr<tgfx::Typeface> FontManager::getTypefaceWithoutFallback(
    const std::string& fontFamily, const std::string& fontStyle) {
  std::shared_ptr<tgfx::Typeface> typeface = getTypefaceFromCache(fontFamily, fontStyle);
  if (typeface == nullptr) {
    typeface = MakeTypefaceWithName(fontFamily, fontStyle);
  }
  if (typeface == nullptr) {
    auto index = fontFamily.find(' ');
    if (index != std::string::npos) {
      auto family = fontFamily.substr(0, index);
      auto style = fontFamily.substr(index + 1);
      typeface = MakeTypefaceWithName(family, style);
    }
  }
  return typeface;
}

std::shared_ptr<tgfx::Typeface> FontManager::getFallbackTypeface(const std::string& name,
                                                                 tgfx::GlyphID* glyphID) {
  std::lock_guard<std::mutex> autoLock(locker);
  for (auto& holder : fallbackFontList) {
    auto typeface = holder->getTypeface();
    if (typeface != nullptr) {
      *glyphID = typeface->getGlyphID(name);
      if (*glyphID != 0) {
        return typeface;
      }
    }
  }
  return tgfx::Typeface::MakeDefault();
}

void FontManager::setFallbackFontNames(const std::vector<std::string>& fontNames) {
  std::lock_guard<std::mutex> autoLock(locker);
  fallbackFontList.clear();
  for (auto& fontFamily : fontNames) {
    auto holder = TypefaceHolder::MakeFromName(fontFamily, "");
    fallbackFontList.push_back(holder);
  }
}

void FontManager::setFallbackFontPaths(const std::vector<std::string>& fontPaths,
                                       const std::vector<int>& ttcIndices) {
  std::lock_guard<std::mutex> autoLock(locker);
  fallbackFontList.clear();
  int index = 0;
  for (auto& fontPath : fontPaths) {
    auto holder = TypefaceHolder::MakeFromFile(fontPath, ttcIndices[index]);
    fallbackFontList.push_back(holder);
    index++;
  }
}

std::shared_ptr<tgfx::Typeface> FontManager::getTypefaceFromCache(const std::string& fontFamily,
                                                                  const std::string& fontStyle) {
  std::lock_guard<std::mutex> autoLock(locker);
  auto result = registeredFontMap.find(PAGFontRegisterKey(fontFamily, fontStyle));
  if (result != registeredFontMap.end()) {
    return result->second;
  }
  return nullptr;
}

static FontManager fontManager = {};

std::shared_ptr<tgfx::Typeface> FontManager::GetTypefaceWithoutFallback(
    const std::string& fontFamily, const std::string& fontStyle) {
  return fontManager.getTypefaceWithoutFallback(fontFamily, fontStyle);
}

static bool RegisterFallbackFonts() {
  if (fontManager.hasFallbackFonts()) {
    return false;
  }
  return Platform::Current()->registerFallbackFonts();
}

std::shared_ptr<tgfx::Typeface> FontManager::GetFallbackTypeface(const std::string& name,
                                                                 tgfx::GlyphID* glyphID) {
  static auto registered = RegisterFallbackFonts();
  USE(registered);
  return fontManager.getFallbackTypeface(name, glyphID);
}

PAGFont FontManager::RegisterFont(const std::string& fontPath, int ttcIndex,
                                  const std::string& fontFamily, const std::string& fontStyle) {
  return fontManager.registerFont(fontPath, ttcIndex, fontFamily, fontStyle);
}

PAGFont FontManager::RegisterFont(const void* data, size_t length, int ttcIndex,
                                  const std::string& fontFamily, const std::string& fontStyle) {
  return fontManager.registerFont(data, length, ttcIndex, fontFamily, fontStyle);
}

void FontManager::UnregisterFont(const PAGFont& font) {
  return fontManager.unregisterFont(font);
}

void FontManager::SetFallbackFontNames(const std::vector<std::string>& fontNames) {
  fontManager.setFallbackFontNames(fontNames);
}

void FontManager::SetFallbackFontPaths(const std::vector<std::string>& fontPaths,
                                       const std::vector<int>& ttcIndices) {
  fontManager.setFallbackFontPaths(fontPaths, ttcIndices);
}
}  // namespace pag
