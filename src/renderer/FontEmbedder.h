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

namespace pagx {

/**
 * FontEmbedder extracts glyph data from laid-out Text nodes and embeds it into the PAGXDocument.
 * It creates Font nodes with glyph paths/images and updates Text nodes with GlyphRun data.
 * The document must have applyLayout() called before embedding.
 *
 * Font merging strategy:
 * - All vector glyphs (with path) are merged into a single Font node
 * - Bitmap glyphs (with image) are grouped by typeface into separate Font nodes
 */
class FontEmbedder {
 public:
  FontEmbedder() = default;

  /**
   * Embeds font data into the document by collecting TextBlobs from all Text nodes.
   * The document must have had applyLayout() called first so that Text nodes contain valid
   * TextBlob data.
   *
   * @param document The document to embed fonts into (modified in place).
   * @return true if embedding succeeded, false otherwise.
   */
  bool embed(PAGXDocument* document);
};

}  // namespace pagx
