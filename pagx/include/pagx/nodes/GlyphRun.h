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

#include <cstdint>
#include <string>
#include <vector>
#include "pagx/nodes/Node.h"
#include "pagx/types/Point.h"

namespace pagx {

class Font;

/**
 * GlyphRun defines pre-shaped glyph data for a segment of text. Each GlyphRun independently
 * references a font resource.
 */
class GlyphRun : public Node {
 public:
  /**
   * Reference to the Font resource.
   */
  Font* font = nullptr;

  /**
   * Font size for rendering. Actual scale = fontSize / font.unitsPerEm. The default value is 12.
   */
  float fontSize = 12.0f;

  /**
   * GlyphID sequence. GlyphID 0 indicates missing glyph (not rendered).
   */
  std::vector<uint16_t> glyphs = {};

  /**
   * Overall X offset. The default value is 0.
   */
  float x = 0.0f;

  /**
   * Overall Y offset. The default value is 0.
   */
  float y = 0.0f;

  /**
   * Per-glyph X offsets. Can be combined with positions.
   * Final X = x + xOffsets[i] + positions[i].x
   */
  std::vector<float> xOffsets = {};

  /**
   * Per-glyph (x, y) offsets. Can be combined with x, y, and xOffsets.
   * Final X = x + xOffsets[i] + positions[i].x
   * Final Y = y + positions[i].y
   */
  std::vector<Point> positions = {};

  /**
   * Per-glyph anchor point offsets relative to the default anchor (advance * 0.5, 0).
   * The anchor is the center point for scale, rotation, and skew transforms.
   */
  std::vector<Point> anchors = {};

  /**
   * Per-glyph scale factors (scaleX, scaleY). Default is (1, 1).
   * Scaling is applied around the anchor point.
   */
  std::vector<Point> scales = {};

  /**
   * Per-glyph rotation angles in degrees. Default is 0.
   * Rotation is applied around the anchor point.
   */
  std::vector<float> rotations = {};

  /**
   * Per-glyph skew angles in degrees (along vertical axis). Default is 0.
   * Skewing is applied around the anchor point.
   */
  std::vector<float> skews = {};

  NodeType nodeType() const override {
    return NodeType::GlyphRun;
  }

 private:
  GlyphRun() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
