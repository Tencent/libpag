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

#include "ShapedText.h"
#include "pagx/PAGXDocument.h"

namespace pagx {

/**
 * FontEmbedder extracts glyph data from ShapedTextMap and embeds it into the PAGXDocument.
 * It creates Font nodes with glyph paths/images and updates Text nodes with GlyphRun data.
 *
 * Font merging strategy:
 * - All vector glyphs (with path) are merged into a single Font node
 * - Bitmap glyphs (with image) are grouped by typeface into separate Font nodes
 */
class FontEmbedder {
 public:
  FontEmbedder() = default;

  /**
   * Embeds font data from ShapedTextMap into the document.
   *
   * This method:
   * 1. Iterates all TextBlobs in shapedTextMap, extracts glyph data (paths or images)
   * 2. Merges glyphs into Font nodes (one for vector, one for bitmap)
   * 3. Creates GlyphRun nodes for each Text, referencing the embedded fonts
   *
   * @param document The document to embed fonts into (modified in place).
   * @param shapedTextMap The typesetting results containing Text -> ShapedText mappings.
   * @param textOrder Stable Text iteration order from typesetting.
   * @return true if embedding succeeded, false otherwise.
   */
  bool embed(PAGXDocument* document, const ShapedTextMap& shapedTextMap,
             const std::vector<Text*>& textOrder);
};

}  // namespace pagx
