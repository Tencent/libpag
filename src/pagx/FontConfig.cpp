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

namespace pagx {

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

void FontConfig::registerTypeface(std::shared_ptr<PAGXTypeface> typeface) {
  if (typeface == nullptr) {
    return;
  }
  Data::FontKey key = {};
  key.family = typeface->fontFamily();
  key.style = typeface->fontStyle();
  data->registeredTypefaces[key] = std::move(typeface);
}

void FontConfig::addFallbackTypeface(std::shared_ptr<PAGXTypeface> typeface) {
  if (typeface == nullptr) {
    return;
  }
  data->fallbackTypefaces.push_back(std::move(typeface));
}

}  // namespace pagx
