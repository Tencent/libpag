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

#include "pagx/FontConfig.h"
#include "FontConfigData.h"
#include "tgfx/core/Typeface.h"

namespace pagx {

TypefaceHolder::TypefaceHolder(std::shared_ptr<tgfx::Typeface> typeface)
    : typeface(std::move(typeface)) {
  if (this->typeface != nullptr) {
    fontFamily = this->typeface->fontFamily();
    fontStyle = this->typeface->fontStyle();
  }
}

TypefaceHolder::TypefaceHolder(std::string path, int ttcIndex, std::string fontFamily,
                               std::string fontStyle)
    : path(std::move(path)), fontFamily(std::move(fontFamily)), fontStyle(std::move(fontStyle)),
      ttcIndex(ttcIndex) {
}

std::shared_ptr<tgfx::Typeface> TypefaceHolder::getTypeface() {
  if (typeface == nullptr) {
    if (!path.empty()) {
      typeface = tgfx::Typeface::MakeFromPath(path, ttcIndex);
    } else if (!fontFamily.empty()) {
      typeface = tgfx::Typeface::MakeFromName(fontFamily, fontStyle);
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

void FontConfig::registerTypeface(std::shared_ptr<tgfx::Typeface> typeface) {
  if (typeface == nullptr) {
    return;
  }
  Data::FontKey key = {};
  key.family = typeface->fontFamily();
  key.style = typeface->fontStyle();
  data->registeredTypefaces[key] = std::move(typeface);
}

void FontConfig::addFallbackTypefaces(std::vector<std::shared_ptr<tgfx::Typeface>> typefaces) {
  for (auto& tf : typefaces) {
    data->fallbackTypefaces.emplace_back(std::move(tf));
  }
}

void FontConfig::addFallbackFont(const std::string& path, int ttcIndex,
                                 const std::string& fontFamily, const std::string& fontStyle) {
  data->fallbackTypefaces.emplace_back(path, ttcIndex, fontFamily, fontStyle);
}

// Lower is preferred. Regular < Medium < Normal < everything else. Shared with LayoutContext's
// legacy findTypeface path; keeping the definition here means any new caller (FontProvider
// adapter, future Inflater fallback) gets the same tie-breaking automatically.
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

std::shared_ptr<tgfx::Typeface> FontConfig::findTypeface(const std::string& fontFamily,
                                                         const std::string& fontStyle) {
  if (fontFamily.empty()) {
    return nullptr;
  }

  // (1) Exact family+style match among registered typefaces. An empty incoming style is
  // interpreted as "Regular" (matches the LayoutContext convention) so callers that only know the
  // family still find the most common face.
  Data::FontKey key = {};
  key.family = fontFamily;
  key.style = fontStyle.empty() ? "Regular" : fontStyle;
  auto it = data->registeredTypefaces.find(key);
  if (it != data->registeredTypefaces.end()) {
    return it->second;
  }

  // (2) Family-name match across registered typefaces, prefer Regular/Medium/Normal. Ties break
  // on style string ordering so the result is deterministic across runs.
  std::shared_ptr<tgfx::Typeface> bestTypeface = nullptr;
  int bestPriority = 4;
  std::string bestStyle = {};
  for (const auto& pair : data->registeredTypefaces) {
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

  // (3) Family-name match among user fallback typefaces. Lazy-loads deferred holders; a holder
  // that fails to load is skipped silently and we keep searching.
  for (auto& holder : data->fallbackTypefaces) {
    if (holder.getFontFamily() != fontFamily) {
      continue;
    }
    auto typeface = holder.getTypeface();
    if (typeface != nullptr) {
      return typeface;
    }
  }
  return nullptr;
}

std::vector<std::shared_ptr<tgfx::Typeface>> FontConfig::fallbackTypefaces() {
  std::vector<std::shared_ptr<tgfx::Typeface>> out;
  out.reserve(data->fallbackTypefaces.size());
  for (auto& holder : data->fallbackTypefaces) {
    auto typeface = holder.getTypeface();
    if (typeface != nullptr) {
      out.push_back(std::move(typeface));
    }
  }
  return out;
}

}  // namespace pagx
