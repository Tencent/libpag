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

#include "pagx/FontConfig.h"
#include <memory>
#include "FontConfigData.h"
#include "SystemFonts.h"
#include "pagx/types/Data.h"
#include "tgfx/core/Typeface.h"

namespace pagx {

// Normalizes an empty style to "Regular" so the stored registration key matches the lookup key
// built by LayoutContext::findTypeface, which applies the same normalization at query time.
static const std::string& NormalizeStyle(const std::string& style) {
  static const std::string regular = "Regular";
  return style.empty() ? regular : style;
}

TypefaceHolder::TypefaceHolder(std::string path, int ttcIndex, std::string family,
                               std::string style)
    : path(std::move(path)), ttcIndex(ttcIndex), fontFamily(std::move(family)),
      fontStyle(std::move(style)) {
}

TypefaceHolder::TypefaceHolder(std::shared_ptr<const std::vector<uint8_t>> bytes, int ttcIndex,
                               std::string family, std::string style)
    : bytes(std::move(bytes)), ttcIndex(ttcIndex), fontFamily(std::move(family)),
      fontStyle(std::move(style)) {
}

TypefaceHolder::TypefaceHolder(std::shared_ptr<tgfx::Typeface> typeface, std::string family,
                               std::string style)
    : fontFamily(std::move(family)), fontStyle(std::move(style)), typeface(std::move(typeface)) {
}

std::shared_ptr<tgfx::Typeface> TypefaceHolder::getTypeface() {
  if (typeface == nullptr) {
    if (!path.empty()) {
      typeface = tgfx::Typeface::MakeFromPath(path, ttcIndex);
    } else if (bytes != nullptr && !bytes->empty()) {
      typeface = tgfx::Typeface::MakeFromBytes(bytes->data(), bytes->size(), ttcIndex);
    }
  }
  return typeface;
}

const std::string& TypefaceHolder::getFontFamily() const {
  return fontFamily;
}

const std::string& TypefaceHolder::getFontStyle() const {
  return fontStyle;
}

const std::string& TypefaceHolder::getPath() const {
  return path;
}

int TypefaceHolder::getTtcIndex() const {
  return ttcIndex;
}

const std::shared_ptr<const std::vector<uint8_t>>& TypefaceHolder::getBytes() const {
  return bytes;
}

FontConfig::FontConfig() : data(std::make_unique<Data>()) {
}

FontConfig::~FontConfig() = default;

FontConfig::FontConfig(const FontConfig& other) : data(std::make_unique<Data>(*other.data)) {
}

FontConfig& FontConfig::operator=(const FontConfig& other) {
  if (this != &other) {
    data = std::make_unique<Data>(*other.data);
  }
  return *this;
}

FontConfig::FontConfig(FontConfig&& other) noexcept = default;
FontConfig& FontConfig::operator=(FontConfig&& other) noexcept = default;

void FontConfig::registerFont(const PAGFont& font, const std::string& path, int ttcIndex) {
  registerFont(path, ttcIndex, font.fontFamily, font.fontStyle);
}

void FontConfig::registerFont(const PAGFont& font, const void* bytes, size_t length, int ttcIndex) {
  registerFont(bytes, length, ttcIndex, font.fontFamily, font.fontStyle);
}

void FontConfig::registerFont(const std::string& path, int ttcIndex, const std::string& fontFamily,
                              const std::string& fontStyle) {
  if (fontFamily.empty()) {
    // Reverse-lookup: build typeface now to read its family/style, then hold the typeface directly
    // to avoid a second lazy build.
    auto typeface = tgfx::Typeface::MakeFromPath(path, ttcIndex);
    if (typeface == nullptr) {
      return;
    }
    Data::FontKey key = {};
    key.family = typeface->fontFamily();
    key.style = typeface->fontStyle();
    data->registeredTypefaces.insert_or_assign(
        key, TypefaceHolder(std::move(typeface), key.family, key.style));
  } else {
    auto normalizedStyle = NormalizeStyle(fontStyle);
    Data::FontKey key = {};
    key.family = fontFamily;
    key.style = normalizedStyle;
    data->registeredTypefaces.insert_or_assign(
        key, TypefaceHolder(path, ttcIndex, fontFamily, normalizedStyle));
  }
}

void FontConfig::registerFont(const void* bytes, size_t length, int ttcIndex,
                              const std::string& fontFamily, const std::string& fontStyle) {
  if (fontFamily.empty()) {
    auto typeface = tgfx::Typeface::MakeFromBytes(bytes, length, ttcIndex);
    if (typeface == nullptr) {
      return;
    }
    Data::FontKey key = {};
    key.family = typeface->fontFamily();
    key.style = typeface->fontStyle();
    data->registeredTypefaces.insert_or_assign(
        key, TypefaceHolder(std::move(typeface), key.family, key.style));
  } else {
    auto normalizedStyle = NormalizeStyle(fontStyle);
    Data::FontKey key = {};
    key.family = fontFamily;
    key.style = normalizedStyle;
    data->registeredTypefaces.insert_or_assign(
        key, TypefaceHolder(std::make_shared<const std::vector<uint8_t>>(
                                static_cast<const uint8_t*>(bytes),
                                static_cast<const uint8_t*>(bytes) + length),
                            ttcIndex, fontFamily, normalizedStyle));
  }
}

void FontConfig::addFallbackFont(const PAGFont& font, const std::string& path, int ttcIndex) {
  addFallbackFont(path, ttcIndex, font.fontFamily, font.fontStyle);
}

void FontConfig::addFallbackFont(const PAGFont& font, const void* bytes, size_t length,
                                 int ttcIndex) {
  addFallbackFont(bytes, length, ttcIndex, font.fontFamily, font.fontStyle);
}

void FontConfig::addFallbackFont(const std::string& path, int ttcIndex,
                                 const std::string& fontFamily, const std::string& fontStyle) {
  if (fontFamily.empty()) {
    auto typeface = tgfx::Typeface::MakeFromPath(path, ttcIndex);
    if (typeface == nullptr) {
      return;
    }
    auto family = typeface->fontFamily();
    auto style = typeface->fontStyle();
    data->fallbackTypefaces.emplace_back(std::move(typeface), std::move(family), std::move(style));
  } else {
    auto normalizedStyle = NormalizeStyle(fontStyle);
    data->fallbackTypefaces.emplace_back(path, ttcIndex, fontFamily, normalizedStyle);
  }
}

void FontConfig::addFallbackFont(const void* bytes, size_t length, int ttcIndex,
                                 const std::string& fontFamily, const std::string& fontStyle) {
  if (fontFamily.empty()) {
    auto typeface = tgfx::Typeface::MakeFromBytes(bytes, length, ttcIndex);
    if (typeface == nullptr) {
      return;
    }
    auto family = typeface->fontFamily();
    auto style = typeface->fontStyle();
    data->fallbackTypefaces.emplace_back(std::move(typeface), std::move(family), std::move(style));
  } else {
    auto normalizedStyle = NormalizeStyle(fontStyle);
    data->fallbackTypefaces.emplace_back(
        std::make_shared<const std::vector<uint8_t>>(static_cast<const uint8_t*>(bytes),
                                                     static_cast<const uint8_t*>(bytes) + length),
        ttcIndex, fontFamily, normalizedStyle);
  }
}

bool FontConfig::registerSystemFont(const std::string& fontFamily, const std::string& fontStyle) {
  auto typeface = SystemFonts::ResolveTypeface(fontFamily, fontStyle);
  if (typeface == nullptr) {
    return false;
  }
  auto normalizedStyle = NormalizeStyle(fontStyle);
  Data::FontKey key = {};
  key.family = fontFamily;
  key.style = normalizedStyle;
  data->registeredTypefaces.insert_or_assign(
      key, TypefaceHolder(std::move(typeface), fontFamily, normalizedStyle));
  return true;
}

bool FontConfig::registerSystemFont(const PAGFont& font) {
  return registerSystemFont(font.fontFamily, font.fontStyle);
}

bool FontConfig::addFallbackSystemFont(const std::string& fontFamily,
                                       const std::string& fontStyle) {
  auto typeface = SystemFonts::ResolveTypeface(fontFamily, fontStyle);
  if (typeface == nullptr) {
    return false;
  }
  auto normalizedStyle = NormalizeStyle(fontStyle);
  data->fallbackTypefaces.emplace_back(std::move(typeface), fontFamily, normalizedStyle);
  return true;
}

bool FontConfig::addFallbackSystemFont(const PAGFont& font) {
  return addFallbackSystemFont(font.fontFamily, font.fontStyle);
}

std::vector<std::string> FontConfig::fallbackFamilyNames() const {
  std::vector<std::string> names = {};
  names.reserve(data->fallbackTypefaces.size());
  for (auto& holder : data->fallbackTypefaces) {
    names.push_back(holder.getFontFamily());
  }
  return names;
}

std::vector<FontSourceInfo> FontConfig::fontSources(const tgfx::Typeface* typeface) {
  std::vector<FontSourceInfo> sources = {};
  if (typeface == nullptr) {
    return sources;
  }
  // The same source can be registered both as a primary font and on the fallback chain (the CLI
  // registers every --fallback file both ways), so one typeface may yield duplicate entries; the
  // caller de-duplicates.
  auto collect = [&](TypefaceHolder& holder) {
    if (holder.getTypeface().get() != typeface) {
      return;
    }
    FontSourceInfo info = {};
    info.path = holder.getPath();
    info.ttcIndex = holder.getTtcIndex();
    const auto& bytes = holder.getBytes();
    if (bytes && !bytes->empty()) {
      // Fully qualified: FontConfig::Data (the registry struct) shadows pagx::Data inside members.
      info.data = pagx::Data::MakeWithCopy(bytes->data(), bytes->size());
    } else if (info.path.empty()) {
      return;
    }
    sources.push_back(std::move(info));
  };
  for (auto& pair : data->registeredTypefaces) {
    collect(pair.second);
  }
  for (auto& holder : data->fallbackTypefaces) {
    collect(holder);
  }
  return sources;
}

bool FontConfig::containsFamily(const std::string& fontFamily) const {
  if (fontFamily.empty()) {
    return false;
  }
  for (const auto& pair : data->registeredTypefaces) {
    if (pair.first.family == fontFamily) {
      return true;
    }
  }
  for (const auto& holder : data->fallbackTypefaces) {
    if (holder.getFontFamily() == fontFamily) {
      return true;
    }
  }
  return false;
}

}  // namespace pagx
