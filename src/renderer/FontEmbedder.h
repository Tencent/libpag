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

#include <set>
#include <string>
#include "pagx/PAGXDocument.h"

namespace tgfx {
class Typeface;
}

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
   * Optional inputs for embed(). Every on-disk typeface that actually contributed glyphs to
   * this embed additionally gets a source-declaration Font node (file="..." with a path
   * relative to `outputBaseDir`, glyphs left empty), so downstream consumers can re-shape with
   * the exact same font files. The typefaces are matched against the document's own fontConfig
   * — the copy applyLayout() kept — because that registry holds the very typeface instances the
   * shaper used (tgfx::Typeface::MakeFromPath returns a fresh instance per call, so matching
   * against a caller-side registry would never hit). Registrations backed by in-memory bytes or
   * system typefaces have no file path and produce no source nodes. Re-embedding a document
   * that already carries matching source nodes is idempotent: existing nodes are reused, not
   * duplicated.
   */
  struct EmbedOptions {
    /**
     * Directory the exported PAGX file will live in; source-declaration `file` attributes are
     * written relative to it. Empty keeps the registered path verbatim.
     */
    std::string outputBaseDir = {};
  };

  /**
   * Resets previously-embedded font data in the document so it can be re-embedded from scratch.
   * Clears the embedded GlyphRuns vector on every Text node and removes previously-installed
   * Font nodes (along with their Glyph, PathData, and Image children) plus any orphan GlyphRun
   * nodes from document->nodes. Font nodes with a non-empty `file` attribute are preserved
   * (only their Glyph children are cleared); Font nodes without `file` are removed entirely.
   * Call this before applyLayout() when re-embedding a file that
   * already has embedded fonts, so that layout performs runtime shaping instead of using stale
   * embedded data.
   *
   * Warning: any external pointers or IDs referencing the Image or PathData nodes previously
   * installed by a font-embed pass will become dangling after this call.
   */
  static void ClearEmbeddedGlyphRuns(PAGXDocument* document);

  /**
   * Embeds font data into the document by collecting layout glyph runs from all Text nodes.
   * The document must have had applyLayout() called first so that Text nodes contain valid
   * layout run data.
   */
  bool embed(PAGXDocument* document, const EmbedOptions& options);

  /**
   * Overload equivalent to embed(document, EmbedOptions{}): embeds without writing
   * source-declaration nodes. (A default argument cannot be used on the overload above because
   * the EmbedOptions default member initializers are not yet usable within the class definition.)
   */
  bool embed(PAGXDocument* document) {
    return embed(document, EmbedOptions{});
  }

 private:
  static void WriteFontSourceDeclarations(PAGXDocument* document,
                                          const std::set<const tgfx::Typeface*>& usedTypefaces,
                                          const EmbedOptions& options, int& fontIndex);
};

}  // namespace pagx
