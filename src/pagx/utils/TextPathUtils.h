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
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include "pagx/types/Matrix.h"
#include "pagx/types/Point.h"

namespace pagx {

class TextPath;

/**
 * A glyph input for TextPath layout: its current matrix, local anchor and horizontal advance.
 * All values are in the TextPath's local coordinate space.
 */
struct TextPathGlyphInput {
  Matrix transform = {};
  Point anchor = {};
  float advance = 0;
};

/**
 * Applies `textPath` to the supplied glyphs and writes one final matrix per input. Mirrors
 * tgfx::TextPath (the single source of truth) including normal/force modes, reversed distance,
 * closed-path wrap, open-path tangent extension and perpendicular rotation. Returns false when the
 * path or its measured length is degenerate, in which case the glyphs stay unmodified.
 */
bool ApplyTextPathToGlyphs(const TextPath& textPath, const std::vector<TextPathGlyphInput>& glyphs,
                           std::vector<Matrix>* outTransforms);

}  // namespace pagx
