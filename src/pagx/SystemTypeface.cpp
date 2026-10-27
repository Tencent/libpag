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

#include <cctype>
#include "SystemFonts.h"
#include "tgfx/core/Typeface.h"

namespace pagx {

std::string SystemFonts::NormalizeFontName(const std::string& name) {
  std::string result = {};
  result.reserve(name.size());
  bool pendingSpace = false;
  for (unsigned char character : name) {
    if (std::isspace(character)) {
      pendingSpace = !result.empty();
      continue;
    }
    if (pendingSpace) {
      result.push_back(' ');
      pendingSpace = false;
    }
    result.push_back(static_cast<char>(std::tolower(character)));
  }
  return result;
}

bool SystemFonts::FontNamesMatch(const std::string& requested, const std::string& resolved) {
  return !resolved.empty() && NormalizeFontName(requested) == NormalizeFontName(resolved);
}

static bool TypefaceMatches(const std::shared_ptr<tgfx::Typeface>& typeface,
                            const std::string& family, const std::string& style,
                            bool requireStyle) {
  if (typeface == nullptr || !SystemFonts::FontNamesMatch(family, typeface->fontFamily())) {
    return false;
  }
  return !requireStyle || style.empty() ||
         SystemFonts::FontNamesMatch(style, typeface->fontStyle());
}

static std::shared_ptr<tgfx::Typeface> LoadTypeface(const FontLocation& location) {
  if (location.path.empty()) {
    return nullptr;
  }
  return tgfx::Typeface::MakeFromPath(location.path, location.ttcIndex);
}

std::shared_ptr<tgfx::Typeface> SystemFonts::ResolveTypeface(const std::string& family,
                                                             const std::string& style) {
  if (family.empty()) {
    return nullptr;
  }

  auto typeface = tgfx::Typeface::MakeFromName(family, style);
  if (TypefaceMatches(typeface, family, style, true)) {
    return typeface;
  }

  auto location = FindFont(family, style);
  typeface = LoadTypeface(location);
  if (TypefaceMatches(typeface, family, style, true)) {
    return typeface;
  }

  if (style.empty()) {
    return nullptr;
  }

  // CSS selects another available face from the requested family when that family does not
  // contain the requested style. Match that behavior without accepting a different family.
  typeface = tgfx::Typeface::MakeFromName(family, "");
  if (TypefaceMatches(typeface, family, style, false)) {
    return typeface;
  }

  location = FindFont(family, "");
  typeface = LoadTypeface(location);
  if (TypefaceMatches(typeface, family, style, false)) {
    return typeface;
  }
  return nullptr;
}

}  // namespace pagx
