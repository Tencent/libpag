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

#pragma once

#include <string>
#include <vector>

namespace pagx {

/**
 * Describes a font location for deferred loading. Stores enough information to create a Typeface
 * on demand without loading the font file upfront.
 */
struct FontLocation {
  std::string path = {};
  std::string fontFamily = {};
  std::string fontStyle = {};
  int ttcIndex = 0;
};

/**
 * Describes a single system font family and its available styles. Returned by
 * SystemFonts::AllFontFamilies for font enumeration purposes.
 */
struct FontFamilyEntry {
  std::string family = {};
  std::vector<std::string> styles = {};
};

/**
 * Provides access to system font metadata — fallback typefaces used during text rendering and
 * the full list of installed font families. On macOS, this uses CoreText
 * (CTFontCopyDefaultCascadeListForLanguages / CTFontManagerCopyAvailableFontFamilyNames). On
 * Linux, this uses fontconfig (FcFontSort / FcFontList). On Windows, this uses DirectWrite
 * (IDWriteFontCollection).
 */
class SystemFonts {
 public:
  /**
   * Returns font location descriptors for the system default fallback fonts, ordered by the user's
   * language preferences. No Typeface objects are created; callers should load fonts on demand.
   */
  static std::vector<FontLocation> FallbackTypefaces();

  /**
   * Returns every installed system font family with its available styles. On failure
   * (platform API unavailable, enumeration fails), returns an empty vector silently.
   * Styles within a family are deduplicated (first-occurrence-wins). Order within
   * styles is platform-native order. Order of families across the vector is NOT
   * guaranteed (callers should sort as needed).
   */
  static std::vector<FontFamilyEntry> AllFontFamilies();
};

}  // namespace pagx
