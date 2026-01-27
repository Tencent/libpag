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

#include <string>
#include <vector>
#include "pagx/nodes/Node.h"
#include "pagx/nodes/Point.h"
#include "pagx/nodes/Matrix.h"
#include "tgfx/core/Font.h"

namespace pagx {

/**
 * RSXform represents a compressed rotation+scale matrix with four components (scos, ssin, tx, ty):
 * | scos  -ssin   tx |
 * | ssin   scos   ty |
 * |   0      0     1 |
 * where scos = scale × cos(angle), ssin = scale × sin(angle).
 */
struct RSXform {
  float scos = 1.0f;
  float ssin = 0.0f;
  float tx = 0.0f;
  float ty = 0.0f;
};

/**
 * GlyphRun defines pre-shaped glyph data for a segment of text. Each GlyphRun independently
 * references a font resource.
 */
class GlyphRun : public Node {
 public:
  /**
   * Reference to Font resource by ID (e.g., "@myFont").
   */
  std::string font = {};

  /**
   * GlyphID sequence. GlyphID 0 indicates missing glyph (not rendered).
   */
  std::vector<tgfx::GlyphID> glyphs = {};

  /**
   * Shared y coordinate for Horizontal positioning mode. The default value is 0.
   */
  float y = 0.0f;

  /**
   * X coordinates for Horizontal positioning mode (each glyph has x, shares y).
   */
  std::vector<float> xPositions = {};

  /**
   * (x, y) coordinates for Point positioning mode.
   */
  std::vector<Point> positions = {};

  /**
   * RSXform transforms for RSXform positioning mode (path text).
   */
  std::vector<RSXform> xforms = {};

  /**
   * Full 2D affine matrices for Matrix positioning mode.
   */
  std::vector<Matrix> matrices = {};

  NodeType nodeType() const override {
    return NodeType::GlyphRun;
  }
};

}  // namespace pagx
