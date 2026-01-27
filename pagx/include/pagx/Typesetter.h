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
#include <vector>
#include "pagx/PAGXDocument.h"
#include "tgfx/core/Typeface.h"

namespace pagx {

/**
 * Typesetter performs text typesetting on PAGXDocument, converting Text elements to pre-shaped
 * format with embedded glyph data. It handles both text shaping (glyph mapping) and text layout
 * (alignment, line breaking, etc.).
 *
 * Terminology:
 * - Pre-shaped: Text has been typeset with embedded GlyphRun data
 * - Runtime shaping: Text needs to be shaped at render time (no GlyphRun data)
 *
 * The typesetter processes text in Group granularity:
 * - If any Text in a Group lacks GlyphRun data, the entire Group is typeset
 * - This ensures consistent TextLayout calculations within the same Group
 */
class Typesetter {
 public:
  /**
   * Creates a Typesetter instance.
   */
  static std::shared_ptr<Typesetter> Make();

  virtual ~Typesetter() = default;

  /**
   * Registers a typeface for a specific font family and style. When typesetting text, the
   * registered typeface will be used if its family and style match the text's fontFamily and
   * fontStyle.
   * @param typeface The typeface to register.
   */
  virtual void registerTypeface(std::shared_ptr<tgfx::Typeface> typeface) = 0;

  /**
   * Sets the fallback typefaces used when no registered typeface matches the text's font
   * properties. The first matching typeface in the list will be used.
   * @param typefaces Fallback typefaces in priority order.
   */
  virtual void setFallbackTypefaces(std::vector<std::shared_ptr<tgfx::Typeface>> typefaces) = 0;

  /**
   * Typesets all text elements in the document. For each Text element, generates GlyphRun data
   * with positioned glyphs. TextLayout modifiers are processed to bake alignment and layout
   * adjustments into the GlyphRun positions. Font resources are created for each unique typeface
   * used, containing glyph path data.
   *
   * @param document The document to process (modified in place).
   * @param force If true, forces re-typesetting of all text elements even if they already have
   *              GlyphRun data. If false (default), only typesets Groups where any Text lacks
   *              GlyphRun data.
   * @return true if any text was typeset, false if no text elements found or all skipped.
   */
  virtual bool typeset(PAGXDocument* document, bool force = false) = 0;
};

}  // namespace pagx
