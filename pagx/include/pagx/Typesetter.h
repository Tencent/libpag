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
#include <unordered_map>
#include <vector>
#include "pagx/PAGXDocument.h"
#include "pagx/TextGlyphs.h"
#include "tgfx/core/Typeface.h"

namespace pagx {

/**
 * Typesetter performs text typesetting on PAGXDocument, converting Text elements into positioned
 * glyph data (TextBlob). It handles font matching, fallback, text shaping, and layout (alignment,
 * line breaking, etc.).
 */
class Typesetter {
 public:
  Typesetter() = default;

  /**
   * Registers a typeface for font matching. When typesetting, registered typefaces are matched
   * first by fontFamily and fontStyle. If no registered typeface matches, the system font is used.
   */
  void registerTypeface(std::shared_ptr<tgfx::Typeface> typeface);

  /**
   * Sets the fallback typefaces used when a character is not found in the primary font (either
   * registered or system). Typefaces are tried in order until one containing the character is found.
   */
  void setFallbackTypefaces(std::vector<std::shared_ptr<tgfx::Typeface>> typefaces);

  /**
   * Creates TextGlyphs for all Text nodes in the document. If a Text node has embedded GlyphRun
   * data (from a loaded PAGX file), it uses that data directly. Otherwise, it performs text
   * shaping using registered/fallback typefaces. TextLayout modifiers are processed to apply
   * alignment, line breaking, and other layout properties.
   * @param document The document containing Text nodes to typeset.
   * @return TextGlyphs containing Text -> TextBlob mappings.
   */
  TextGlyphs createTextGlyphs(PAGXDocument* document);

 private:
  friend class TypesetterContext;

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

  std::unordered_map<FontKey, std::shared_ptr<tgfx::Typeface>, FontKeyHash> registeredTypefaces = {};
  std::vector<std::shared_ptr<tgfx::Typeface>> fallbackTypefaces = {};
};

}  // namespace pagx
