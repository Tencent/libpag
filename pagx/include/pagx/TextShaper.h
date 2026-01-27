/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 THL A29 Limited, a Tencent company. All rights reserved.
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
#include <vector>
#include "pagx/PAGXDocument.h"
#include "tgfx/core/Typeface.h"

namespace pagx {

/**
 * TextShaper converts text elements in a PAGXDocument from runtime shaping format to pre-shaped
 * format. This process:
 *
 * 1. Performs text shaping/layout using the provided fonts
 * 2. Extracts glyph outlines as path data
 * 3. Embeds the path data as Font resources in the document
 * 4. Adds GlyphRun data to Text elements pointing to embedded fonts
 *
 * After shaping, the document can be rendered without runtime font dependencies, ensuring
 * cross-platform consistency. The original text content and font properties are preserved
 * for potential re-editing.
 *
 * Terminology:
 * - Runtime shaping: Text is shaped at render time using system/loaded fonts
 * - Pre-shaped: Text has been shaped ahead of time with embedded glyph outlines
 */
class TextShaper {
 public:
  /**
   * Creates a TextShaper with the specified fallback typefaces.
   * @param fallbackTypefaces Typefaces to use for text shaping, in priority order.
   */
  static std::shared_ptr<TextShaper> Make(
      std::vector<std::shared_ptr<tgfx::Typeface>> fallbackTypefaces);

  virtual ~TextShaper() = default;

  /**
   * Shapes all text elements in the document, converting them from runtime shaping format
   * to pre-shaped format with embedded font resources.
   *
   * For each unique typeface used, a Font resource is added to the document containing
   * the glyph outlines as SVG path data. Text elements are updated with GlyphRun data
   * that references these embedded fonts.
   *
   * @param document The document to process (modified in place).
   * @return true if any text was shaped, false if no text elements found or all already pre-shaped.
   */
  virtual bool shape(PAGXDocument* document) = 0;
};

}  // namespace pagx
