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

#include "pagx/PAGXDocument.h"
#include "pagx/TextGlyphs.h"

namespace pagx {

/**
 * FontEmbedder extracts glyph data from TextGlyphs and embeds it into the PAGXDocument.
 * It creates Font nodes with glyph paths/images and updates Text nodes with GlyphRun data.
 *
 * Font merging strategy:
 * - All vector glyphs (with path) are merged into one Font node
 * - All bitmap glyphs (with image) are merged into another Font node
 * - Maximum 2 Font nodes per document
 */
class FontEmbedder {
 public:
  /**
   * Embeds font data from TextGlyphs into the document.
   *
   * This method:
   * 1. Iterates all TextBlobs in textGlyphs, extracts glyph data (paths or images)
   * 2. Merges glyphs into Font nodes (one for vector, one for bitmap)
   * 3. Creates GlyphRun nodes for each Text, referencing the embedded fonts
   *
   * @param document The document to embed fonts into (modified in place).
   * @param textGlyphs The typesetting results containing Text -> TextBlob mappings.
   * @return true if embedding succeeded, false otherwise.
   */
  static bool Embed(PAGXDocument* document, const TextGlyphs& textGlyphs);
};

}  // namespace pagx
