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
#include <cmath>
#include <mutex>
#include <unordered_map>
#include "SystemFonts.h"
#include "pagx/utils/CSSFontStyle.h"
#include "tgfx/core/Typeface.h"
#ifdef __APPLE__
#include "tgfx/platform/apple/CTTypeface.h"
#endif

namespace pagx {

CSSFontStyle SystemFonts::TypefaceStyle(const tgfx::Typeface* typeface, CSSFontStyle result) {
  if (!typeface) return result;
  auto os2 = typeface->copyTableData(0x4F532F32);  // OS/2
  // OS/2 describes the default instance of a variable font. A native named instance already
  // carries its instantiated traits; replacing them with the default would lose that instance.
  bool variable = typeface->copyTableData(0x66766172) != nullptr;  // fvar
  if (os2 && os2->size() >= 8) {
    auto bytes = static_cast<const uint8_t*>(os2->data());
    auto read = [&](size_t offset) { return (bytes[offset] << 8) | bytes[offset + 1]; };
    auto weight = read(4);
    if (!variable && weight >= 1 && weight <= 1000) result.weight = weight;
    auto width = read(6);
    if (!variable && width >= 1 && width <= 9) result.width = CSSFontWidthFromClass(width);
    if (os2->size() >= 64) {
      auto flags = read(62);
      // Some older fonts omit italic flags. Positive metadata is authoritative; absence is
      // supplemented by native traits below (and by the style label when no native API exists).
      if ((flags & 1) != 0) result.slant = CSSFontSlant::Italic;
      if ((flags & (1 << 9)) != 0) result.slant = CSSFontSlant::Oblique;
    }
  }
#ifdef __APPLE__
  if (auto font = tgfx::CTTypeface::GetCTFont(typeface)) {
    if (CTFontGetSymbolicTraits(font) & kCTFontItalicTrait) {
      if (result.slant == CSSFontSlant::Normal) result.slant = CSSFontSlant::Italic;
    } else {
      result.slant = CSSFontSlant::Normal;
    }
    if (auto variations = CTFontCopyVariation(font)) {
      auto axis = [&](int32_t tag, double* value) {
        auto key = CFNumberCreate(nullptr, kCFNumberSInt32Type, &tag);
        auto number = static_cast<CFNumberRef>(CFDictionaryGetValue(variations, key));
        CFRelease(key);
        return number && CFGetTypeID(number) == CFNumberGetTypeID() &&
               CFNumberGetValue(number, kCFNumberDoubleType, value) && std::isfinite(*value);
      };
      double value = 0;
      if (axis(0x77676874, &value) && value >= 1 && value <= 1000) result.weight = value;  // wght
      if (axis(0x77647468, &value) && value > 0) result.width = value;                     // wdth
      if (axis(0x6974616C, &value) && value > 0) result.slant = CSSFontSlant::Italic;      // ital
      if (axis(0x736C6E74, &value) && value != 0) result.slant = CSSFontSlant::Oblique;    // slnt
      CFRelease(variations);
    }
  }
#endif
  return result;
}

CSSFontStyle SystemFonts::TypefaceStyle(const tgfx::Typeface* typeface) {
  return TypefaceStyle(typeface,
                       typeface ? FontStyleFromName(typeface->fontStyle()) : CSSFontStyle{});
}

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

static const std::vector<CSSFontFace>& FamilyFaces(const std::string& family) {
  static const auto families = SystemFonts::AllFontFamilies();
  static std::mutex mutex;
  static std::unordered_map<std::string, std::vector<CSSFontFace>> cache;
  std::lock_guard<std::mutex> lock(mutex);
  auto key = SystemFonts::NormalizeFontName(family);
  auto found = cache.find(key);
  if (found != cache.end()) return found->second;
  std::vector<CSSFontFace> faces;
  for (const auto& entry : families) {
    if (!SystemFonts::FontNamesMatch(family, entry.family)) continue;
    for (size_t i = 0; i < entry.styles.size(); ++i) {
      const auto& name = entry.styles[i];
      auto style = FontStyleFromName(name);
      for (const auto& face : entry.faces) {
        if (face.name == name) {
          style = face.style;
          break;
        }
      }
      auto typeface = tgfx::Typeface::MakeFromName(entry.family, name);
      if (!TypefaceMatches(typeface, family, name, true))
        typeface = LoadTypeface(SystemFonts::FindFont(entry.family, name));
      if (TypefaceMatches(typeface, family, name, true))
        faces.push_back({name, SystemFonts::TypefaceStyle(typeface.get(), style)});
    }
    break;
  }
  return cache.emplace(std::move(key), std::move(faces)).first->second;
}

std::shared_ptr<tgfx::Typeface> SystemFonts::MatchTypeface(const std::string& family,
                                                           const CSSFontStyle& request) {
  if (family.empty()) return nullptr;
  const auto& faces = FamilyFaces(family);
  auto best = MatchCSSFontFace(faces, request);
  if (best < 0) return nullptr;
  const auto& style = faces[best].name;
  auto typeface = tgfx::Typeface::MakeFromName(family, style);
  if (TypefaceMatches(typeface, family, style, true)) return typeface;
  typeface = LoadTypeface(FindFont(family, style));
  return TypefaceMatches(typeface, family, style, true) ? typeface : nullptr;
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

  typeface = MatchTypeface(family, FontStyleFromName(style));
  if (typeface) return typeface;

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
