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

#include <cstddef>
#include <vector>
#include "pagx/types/Color.h"

namespace pagx {

class TextModifier;

/**
 * Per-glyph paint overrides accumulated from TextModifiers. The alpha of fillColor / strokeColor
 * is the override weight: 0 keeps the original painter, 1 replaces it, and values in between draw
 * the original painter plus a SrcOver overlay.
 */
struct GlyphPaintStyle {
  Color fillColor = {0, 0, 0, 0};
  Color strokeColor = {0, 0, 0, 0};
  float strokeWidth = 0;
  float strokeWidthFactor = 0;
  float alpha = 1;
};

/**
 * Applies the paint channels of `modifier` to glyph styles at [baseIndex, baseIndex + count).
 * `totalCount` is the number of glyphs the modifier's selectors see in the current scope.
 */
void ApplyTextModifierPaint(const TextModifier& modifier, size_t baseIndex, size_t totalCount,
                            std::vector<GlyphPaintStyle>* styles);

/**
 * Returns the stroke width for a glyph style. Mirrors tgfx::StrokeStyle's BlendStrokeWidth so the
 * exported SVG and the renderer agree on strokeWidthFactor interpolation.
 */
float BlendGlyphStrokeWidth(float baseWidth, const GlyphPaintStyle& style);

bool GlyphPaintStylesEqual(const GlyphPaintStyle& a, const GlyphPaintStyle& b);

}  // namespace pagx
