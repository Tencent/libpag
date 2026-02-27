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
#include "ShapedText.h"
#include "pagx/PAGXDocument.h"
#include "tgfx/core/Typeface.h"

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
 * TextLayout performs text layout on PAGXDocument, converting Text elements into positioned glyph
 * data (TextBlob). It handles font matching, fallback, text shaping, and layout (alignment, line
 * breaking, etc.).
 */
class TextLayout {
 public:
  TextLayout() = default;

  /**
   * Registers a typeface for font matching. Registered typefaces are matched first by fontFamily
   * and fontStyle. If no registered typeface matches, the system font is used.
   */
  void registerTypeface(std::shared_ptr<tgfx::Typeface> typeface);

  /**
   * Sets the fallback typefaces used when a character is not found in the primary font (either
   * registered or system). Typefaces are tried in order until one containing the character is found.
   */
  void setFallbackTypefaces(std::vector<std::shared_ptr<tgfx::Typeface>> typefaces);

  /**
   * Adds a deferred fallback font that will be loaded on demand when needed during text shaping.
   * @param path The font file path (empty if using fontFamily for name-based loading).
   * @param ttcIndex The face index within a TTC font collection.
   * @param fontFamily The font family name for matching and name-based loading.
   * @param fontStyle The font style name.
   */
  void addFallbackFont(const std::string& path, int ttcIndex, const std::string& fontFamily,
                       const std::string& fontStyle);

  /**
   * Performs text layout for all Text nodes in the document.
   */
  TextLayoutResult layout(PAGXDocument* document);

 private:
  friend class TextLayoutContext;

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
