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
#include "pagx/nodes/Node.h"
#include "pagx/nodes/Point.h"

namespace pagx {

/**
 * Glyph defines a single glyph's rendering data. Either path or image must be specified, not both.
 */
class Glyph : public Node {
 public:
  /**
   * SVG path data for vector glyph outline. Mutually exclusive with image.
   */
  std::string path = {};

  /**
   * Image data (base64 data URI) or external file path for bitmap glyph. Mutually exclusive with
   * path.
   */
  std::string image = {};

  /**
   * Bitmap offset for image glyphs (only used when image is specified). The default value is (0,0).
   */
  Point offset = {};

  NodeType nodeType() const override {
    return NodeType::Glyph;
  }
};

/**
 * Font defines an embedded font resource containing subsetted glyph data (vector outlines or
 * bitmaps). PAGX files embed glyph data for complete self-containment, ensuring cross-platform
 * rendering consistency.
 */
class Font : public Node {
 public:
  /**
   * The list of glyphs in this font. GlyphID is the index + 1 (GlyphID 0 is reserved for missing
   * glyph).
   */
  std::vector<std::unique_ptr<Glyph>> glyphs = {};

  NodeType nodeType() const override {
    return NodeType::Font;
  }
};

}  // namespace pagx
