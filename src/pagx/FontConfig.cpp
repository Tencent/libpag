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
#include "FontConfigData.h"
#include "tgfx/core/Typeface.h"

namespace pagx {

TypefaceHolder::TypefaceHolder(std::string path, int ttcIndex, std::string family,
                               std::string style)
    : path(std::move(path)), ttcIndex(ttcIndex), fontFamily(std::move(family)),
      fontStyle(std::move(style)) {
}

TypefaceHolder::TypefaceHolder(std::vector<uint8_t> bytes, int ttcIndex, std::string family,
                               std::string style)
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
    } else if (!bytes.empty()) {
      typeface = tgfx::Typeface::MakeFromBytes(bytes.data(), bytes.size(), ttcIndex);
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
    Data::FontKey key = {};
    key.family = fontFamily;
    key.style = fontStyle;
    data->registeredTypefaces.insert_or_assign(
        key, TypefaceHolder(path, ttcIndex, fontFamily, fontStyle));
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
    Data::FontKey key = {};
    key.family = fontFamily;
    key.style = fontStyle;
    data->registeredTypefaces.insert_or_assign(
        key, TypefaceHolder(std::vector<uint8_t>(static_cast<const uint8_t*>(bytes),
                                                 static_cast<const uint8_t*>(bytes) + length),
                            ttcIndex, fontFamily, fontStyle));
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
    data->fallbackTypefaces.emplace_back(path, ttcIndex, fontFamily, fontStyle);
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
    data->fallbackTypefaces.emplace_back(
        std::vector<uint8_t>(static_cast<const uint8_t*>(bytes),
                             static_cast<const uint8_t*>(bytes) + length),
        ttcIndex, fontFamily, fontStyle);
  }
}

bool FontConfig::registerSystemFont(const std::string& fontFamily, const std::string& fontStyle) {
  auto typeface = tgfx::Typeface::MakeFromName(fontFamily, fontStyle);
  if (typeface == nullptr) {
    return false;
  }
  auto family = typeface->fontFamily();
  auto style = typeface->fontStyle();
  Data::FontKey key = {};
  key.family = family;
  key.style = style;
  data->registeredTypefaces.insert_or_assign(
      key, TypefaceHolder(std::move(typeface), std::move(family), std::move(style)));
  return true;
}

bool FontConfig::registerSystemFont(const PAGFont& font) {
  return registerSystemFont(font.fontFamily, font.fontStyle);
}

bool FontConfig::addFallbackSystemFont(const std::string& fontFamily,
                                       const std::string& fontStyle) {
  auto typeface = tgfx::Typeface::MakeFromName(fontFamily, fontStyle);
  if (typeface == nullptr) {
    return false;
  }
  auto family = typeface->fontFamily();
  auto style = typeface->fontStyle();
  data->fallbackTypefaces.emplace_back(std::move(typeface), std::move(family), std::move(style));
  return true;
}

bool FontConfig::addFallbackSystemFont(const PAGFont& font) {
  return addFallbackSystemFont(font.fontFamily, font.fontStyle);
}

}  // namespace pagx
