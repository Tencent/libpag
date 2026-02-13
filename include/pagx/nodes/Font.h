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

#include <vector>
#include "pagx/nodes/Node.h"
#include "pagx/types/Point.h"

namespace pagx {

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
   * The list of glyphs in this font. GlyphID is the index + 1 (GlyphID 0 is reserved for missing
   * glyph).
   */
  std::vector<Glyph*> glyphs = {};

  static bool IsTypeOf(NodeType type) {
    return type == NodeType::Font;
  }

  NodeType nodeType() const override {
    return NodeType::Font;
  }

 private:
  Font() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
