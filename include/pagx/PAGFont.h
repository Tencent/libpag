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

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace pagx {

/**
 * Describes a font requirement by (fontFamily, fontStyle). Returned by
 * PAGXDocument::getRequiredFonts() to enumerate which fonts a document references. The caller
 * resolves each entry to a font file and registers it with FontConfig before applyLayout() or
 * embed().
 */
class PAGFont {
 public:
  PAGFont() = default;
  PAGFont(std::string family, std::string style)
      : fontFamily(std::move(family)), fontStyle(std::move(style)) {}

  /**
   * Registers a font from a file into the process-wide PAGX font registry, so that text in PAGX
   * documents laid out or re-shaped afterwards renders as designed. The registration key
   * (fontFamily and fontStyle) is read from the font file itself. Fonts registered here are
   * visible to every PAGX document: each document's own FontConfig takes precedence, then this
   * registry, then system fonts.
   * @param fontPath The font file path (TTF, OTF, or the first face of a TTC collection).
   * @return True if the font file was parsed and registered successfully.
   */
  static bool RegisterFont(const std::string& fontPath);

  /**
   * Registers a font from an in-memory buffer into the process-wide PAGX font registry, with
   * the same semantics as the path-based overload. The buffer is copied, so the caller may free
   * the data once the call returns.
   * @param data The font data bytes.
   * @param length The byte size of the font data.
   * @return True if the font data was parsed and registered successfully.
   */
  static bool RegisterFont(const void* data, size_t length);

  /**
   * Replaces the process-wide fallback font family names. When a character is missing from the
   * primary typeface, these families are tried in order before system fallback fonts. Names are
   * resolved from the system; unknown names are skipped. Mirrors the legacy
   * PAGFont::SetFallbackFontNames and is intended to be called once during initialization.
   * @param names The fallback font family names, in try order.
   */
  static void SetFallbackFontNames(const std::vector<std::string>& names);

  std::string fontFamily;
  std::string fontStyle;

  bool operator==(const PAGFont& other) const {
    return fontFamily == other.fontFamily && fontStyle == other.fontStyle;
  }
  bool operator<(const PAGFont& other) const {
    if (fontFamily != other.fontFamily) {
      return fontFamily < other.fontFamily;
    }
    return fontStyle < other.fontStyle;
  }
};

}  // namespace pagx
