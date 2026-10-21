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

#include <algorithm>
#include <cctype>
#include "SystemFonts.h"
#include "pagx/utils/CSSFontStyle.h"
#include "tgfx/core/Typeface.h"
#ifdef __APPLE__
#include "tgfx/platform/apple/CTTypeface.h"
#endif

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

#ifdef __APPLE__
static std::shared_ptr<tgfx::Typeface> NativeTypeface(const std::string& family,
                                                      const std::string& style, float size) {
  CTFontRef font = nullptr;
  if (family == "system-ui") {
    font = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, size, nullptr);
    if (font) {
      auto parsed = ParseFontStyleName(style);
      // CoreText trait weights, corresponding to CSS weights 100 through 900.
      static const CGFloat weights[] = {-0.8, -0.6, -0.4, 0, 0.23, 0.3, 0.4, 0.56, 0.62};
      CGFloat weight = weights[std::max(0, std::min(8, parsed.weight / 100 - 1))];
      auto value = CFNumberCreate(nullptr, kCFNumberCGFloatType, &weight);
      const void* keys[] = {kCTFontWeightTrait};
      const void* values[] = {value};
      auto traits = CFDictionaryCreate(nullptr, keys, values, 1, &kCFTypeDictionaryKeyCallBacks,
                                       &kCFTypeDictionaryValueCallBacks);
      const void* attrKeys[] = {kCTFontTraitsAttribute};
      const void* attrValues[] = {traits};
      auto attributes =
          CFDictionaryCreate(nullptr, attrKeys, attrValues, 1, &kCFTypeDictionaryKeyCallBacks,
                             &kCFTypeDictionaryValueCallBacks);
      auto descriptor = CTFontDescriptorCreateWithAttributes(attributes);
      auto weighted = CTFontCreateCopyWithAttributes(font, size, nullptr, descriptor);
      CFRelease(descriptor);
      CFRelease(attributes);
      CFRelease(traits);
      CFRelease(value);
      CFRelease(font);
      font = weighted;
      if (parsed.italic && font) {
        auto italic = CTFontCreateCopyWithSymbolicTraits(font, size, nullptr, kCTFontItalicTrait,
                                                         kCTFontItalicTrait);
        if (italic) {
          CFRelease(font);
          font = italic;
        }
      }
    }
  } else {
    auto cfFamily = CFStringCreateWithCString(nullptr, family.c_str(), kCFStringEncodingUTF8);
    auto cfStyle = CFStringCreateWithCString(nullptr, style.c_str(), kCFStringEncodingUTF8);
    if (!cfFamily || !cfStyle) {
      if (cfFamily) CFRelease(cfFamily);
      if (cfStyle) CFRelease(cfStyle);
      return nullptr;
    }
    const void* keys[] = {kCTFontFamilyNameAttribute, kCTFontStyleNameAttribute};
    const void* values[] = {cfFamily, cfStyle};
    auto attributes =
        CFDictionaryCreate(nullptr, keys, values, style.empty() ? 1 : 2,
                           &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    auto descriptor = CTFontDescriptorCreateWithAttributes(attributes);
    font = CTFontCreateWithFontDescriptor(descriptor, size, nullptr);
    CFRelease(descriptor);
    CFRelease(attributes);
    CFRelease(cfFamily);
    CFRelease(cfStyle);
  }
  if (!font) return nullptr;
  auto result = tgfx::CTTypeface::MakeFromCTFont(font);
  CFRelease(font);
  return result;
}
#endif

std::shared_ptr<tgfx::Typeface> SystemFonts::ResolveTypeface(const std::string& family,
                                                             const std::string& style, float size) {
  if (family.empty()) {
    return nullptr;
  }

#ifdef __APPLE__
  auto typeface = NativeTypeface(family, style, size);
  if (family == "system-ui") return typeface;
#else
  (void)size;
  auto typeface = tgfx::Typeface::MakeFromName(family, style);
#endif
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

  // Select the closest weight/slant within the requested family before falling back to its
  // default face. For example, a family ending at Semibold must not turn a CSS Bold request
  // into Regular. Enumerate lazily; exact face matches above avoid this work entirely.
  static const auto families = AllFontFamilies();
  for (const auto& entry : families) {
    if (!FontNamesMatch(family, entry.family)) continue;
    auto matchedStyle = MatchCSSFontStyle(entry.styles, style);
    if (matchedStyle.empty()) break;
#ifdef __APPLE__
    typeface = NativeTypeface(entry.family, matchedStyle, size);
#else
    typeface = tgfx::Typeface::MakeFromName(entry.family, matchedStyle);
#endif
    if (TypefaceMatches(typeface, entry.family, matchedStyle, true)) return typeface;
    typeface = LoadTypeface(FindFont(entry.family, matchedStyle));
    if (TypefaceMatches(typeface, entry.family, matchedStyle, true)) return typeface;
    break;
  }

#ifdef __APPLE__
  typeface = NativeTypeface(family, "", size);
#else
  typeface = tgfx::Typeface::MakeFromName(family, "");
#endif
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
