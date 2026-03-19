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

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tgfx {
class Typeface;
}

namespace pagx {

/**
 * Holds font location info and creates the Typeface on demand. Once created, the Typeface is
 * cached for subsequent access.
 */
class TypefaceHolder {
 public:
  explicit TypefaceHolder(std::shared_ptr<tgfx::Typeface> typeface);

  TypefaceHolder(std::string path, int ttcIndex, std::string fontFamily, std::string fontStyle);

  std::shared_ptr<tgfx::Typeface> getTypeface();

  const std::string& getFontFamily() const;

  const std::string& getFontStyle() const;

 private:
  std::string path = {};
  std::string fontFamily = {};
  std::string fontStyle = {};
  int ttcIndex = 0;
  std::shared_ptr<tgfx::Typeface> typeface = nullptr;
};

/**
 * FontConfig manages registered and fallback typefaces for font lookup.
 * Provides a unified font discovery mechanism with a 5-level fallback strategy:
 *   1. Exact match in registered typefaces (family + style)
 *   2. Family-name match in registered typefaces (prefer "Regular" style)
 *   3. Family-name match in fallback typefaces
 *   4. First fallback typeface
 *   5. System font lookup
 * This decouples font configuration from text layout and layout computation,
 * allowing multiple components (AutoLayout, TextLayout, LayerBuilder) to share the same font
 * registry.
 */
class FontConfig {
 public:
  FontConfig() = default;

  /**
   * Registers a typeface for exact family+style lookup.
   * Typically used for fonts explicitly provided by the application.
   */
  void registerTypeface(std::shared_ptr<tgfx::Typeface> typeface);

  /**
   * Adds fallback typefaces used when a character is not found in the primary font
   * (either registered or system). Typefaces are tried in order.
   */
  void addFallbackTypefaces(std::vector<std::shared_ptr<tgfx::Typeface>> typefaces);

  /**
   * Adds a deferred fallback font that will be loaded on demand when needed.
   * Supports both file-based loading and name-based system font lookup.
   * @param path The font file path (empty if using fontFamily for name-based loading).
   * @param ttcIndex The face index within a TTC font collection.
   * @param fontFamily The font family name for matching and name-based loading.
   * @param fontStyle The font style name (e.g. "Regular", "Bold").
   */
  void addFallbackFont(const std::string& path, int ttcIndex, const std::string& fontFamily,
                       const std::string& fontStyle);

  /**
   * Finds and returns a typeface matching the requested family and style.
   * Uses the 5-level fallback strategy described above.
   * Returns nullptr if not found (including system font lookup failure).
   */
  std::shared_ptr<tgfx::Typeface> findTypeface(const std::string& fontFamily,
                                               const std::string& fontStyle);

 private:
  friend class TextLayoutContext;  // Allow TextLayoutContext to access fallbackTypefaces

  struct FontKey {
    std::string family = {};
    std::string style = {};

    bool operator==(const FontKey& other) const {
      return family == other.family && style == other.style;
    }
  };

  struct FontKeyHash {
    size_t operator()(const FontKey& key) const {
      return std::hash<std::string>()(key.family) ^ (std::hash<std::string>()(key.style) << 1);
    }
  };

  std::unordered_map<FontKey, std::shared_ptr<tgfx::Typeface>, FontKeyHash> registeredTypefaces =
      {};
  std::vector<TypefaceHolder> fallbackTypefaces = {};
};

}  // namespace pagx
