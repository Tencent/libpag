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
#include <vector>
#include "pagx/nodes/Node.h"
#include "pagx/types/Point.h"

namespace pagx {

struct FontRenderCache;
class GlyphRunRenderer;
class Image;
class PathData;

/**
 * Glyph defines a single glyph's rendering data. Either path or image must be specified, not both.
 */
class Glyph : public Node {
 public:
  /**
   * Vector glyph outline data. Mutually exclusive with image.
   */
  PathData* path = nullptr;

  /**
   * Bitmap glyph image resource. Mutually exclusive with path.
   */
  Image* image = nullptr;

  /**
   * Glyph offset in design space. Typically used for bitmap glyphs. The default value is (0,0).
   */
  Point offset = {};

  /**
   * Horizontal advance width in design space (unitsPerEm coordinates). This value determines the
   * spacing to the next glyph when using Default positioning mode.
   */
  float advance = 0.0f;

  NodeType nodeType() const override {
    return NodeType::Glyph;
  }

 private:
  Glyph() = default;

  friend class PAGXDocument;
};

/**
 * Font defines an embedded font resource containing subsetted glyph data (vector outlines or
 * bitmaps). PAGX files embed glyph data for complete self-containment, ensuring cross-platform
 * rendering consistency.
 */
class Font : public Node {
 public:
  /**
   * Units per em of the font design space. Rendering scale = fontSize / unitsPerEm.
   * The default value is 1000.
   */
  int unitsPerEm = 1000;

  /**
   * Path to an external font file. When set, this Font node serves as a font source declaration:
   * `pagx embed` loads and registers the referenced font for text shaping. Extracted glyph data
   * is stored in separate Font nodes (the source node's `glyphs` is preserved as empty across
   * embed). The path is resolved relative to the PAGX file's directory. An empty string means
   * no external reference.
   *
   * After PAGXImporter::FromFile() resolves relative paths to absolute paths for runtime use,
   * the original verbatim string from the XML attribute is preserved here so that PAGXExporter
   * can round-trip the authored path (relative or URL) unchanged.
   */
  std::string file = {};

  /**
   * The verbatim `file` attribute value as it appeared in the source XML, before any path
   * resolution. PAGXExporter writes this value when non-empty, ensuring that relative paths
   * authored by the user are preserved after a round-trip through embed and re-export.
   */
  std::string fileOriginal = {};

  /**
   * The list of glyphs in this font. GlyphID is the index + 1 (GlyphID 0 is reserved for missing
   * glyph).
   *
   * This list is intended to be populated once at document construction time (typically by
   * FontEmbedder or by loading a PAGX file). Mutations made after the first render call are not
   * picked up: GlyphRunRenderer caches the built typeface (or a build-failed sentinel) on the
   * first call and reuses it for subsequent calls. Modifying glyphs after that point is undefined
   * behavior.
   */
  std::vector<Glyph*> glyphs = {};

  NodeType nodeType() const override {
    return NodeType::Font;
  }

  ~Font() override;

 private:
  Font();

  // Drops every render-side cache entry held by this Font (typeface and the build sentinel).
  // Called by PAGXDocument::clearEmbed so that a subsequent embed() cycle starts from a clean
  // state on Font nodes that survive into the next cycle.
  void resetRenderCache();

  // Lazily-built render cache (typeface + build sentinel). Hidden behind a PIMPL so the public
  // header does not need to expose tgfx types or memory layout to consumers of pagx.
  // Read/write of this cache is not synchronized: callers that share the same Font across
  // threads (e.g. multiple LayerBuilder::Build calls) must serialize externally — see the
  // contract on LayerBuilder::Build.
  std::unique_ptr<FontRenderCache> renderCache;

  friend class PAGXDocument;
  friend class GlyphRunRenderer;
};

}  // namespace pagx
