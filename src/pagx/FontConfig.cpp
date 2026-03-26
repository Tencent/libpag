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

void FontConfig::registerTypeface(std::shared_ptr<tgfx::Typeface> typeface) {
  if (typeface == nullptr) {
    return;
  }
  FontKey key = {};
  key.family = typeface->fontFamily();
  key.style = typeface->fontStyle();
  registeredTypefaces[key] = std::move(typeface);
}

void FontConfig::addFallbackTypefaces(std::vector<std::shared_ptr<tgfx::Typeface>> typefaces) {
  for (auto& tf : typefaces) {
    fallbackTypefaces.emplace_back(std::move(tf));
  }
}

void FontConfig::addFallbackFont(const std::string& path, int ttcIndex,
                                 const std::string& fontFamily, const std::string& fontStyle) {
  fallbackTypefaces.emplace_back(path, ttcIndex, fontFamily, fontStyle);
}

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
  // Stage 1: Exact match in registered typefaces
  if (!fontFamily.empty()) {
    FontKey key = {};
    key.family = fontFamily;
    key.style = fontStyle.empty() ? "Regular" : fontStyle;
    auto it = registeredTypefaces.find(key);
    if (it != registeredTypefaces.end()) {
      return it->second;
    }

    // Stage 2: Family-name match in registered typefaces (prefer Regular style)
    std::shared_ptr<tgfx::Typeface> bestTypeface = nullptr;
    int bestPriority = 4;
    std::string bestStyle = {};
    std::string bestFamily = {};
    for (const auto& pair : registeredTypefaces) {
      if (pair.first.family != fontFamily) {
        continue;
      }
      int priority = StylePriority(pair.first.style);
      bool preferred = (bestTypeface == nullptr) || (priority < bestPriority) ||
                       (priority == bestPriority && pair.first.style < bestStyle) ||
                       (priority == bestPriority && pair.first.style == bestStyle &&
                        pair.first.family < bestFamily);
      if (preferred) {
        bestTypeface = pair.second;
        bestPriority = priority;
        bestStyle = pair.first.style;
        bestFamily = pair.first.family;
      }
    }
    if (bestTypeface != nullptr) {
      return bestTypeface;
    }
  }

  // Stage 3: Family-name match in fallback typefaces
  if (!fontFamily.empty()) {
    for (auto& holder : fallbackTypefaces) {
      if (holder.getFontFamily() == fontFamily) {
        return holder.getTypeface();
      }
    }
  }

  // Stage 4: Use first fallback typeface
  if (!fallbackTypefaces.empty()) {
    return fallbackTypefaces[0].getTypeface();
  }

  // Stage 5: System font lookup
  if (!fontFamily.empty()) {
    return tgfx::Typeface::MakeFromName(fontFamily, fontStyle);
  }

  return nullptr;
}

}  // namespace pagx
