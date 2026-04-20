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

#include <string>
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/ImagePattern.h"

namespace pagx {

// Renders shapes filled with color sources that HTML/CSS cannot express natively
// (DiamondGradient and tiled ImagePattern) into a standalone PNG on disk. Internally spins up a
// short-lived tgfx GL context, builds a minimal one-element PAGXDocument, and reuses the full
// LayerBuilder pipeline so the rasterized result is visually identical to a native PAGX render.
class HTMLStaticImageRenderer {
 public:
  // Rasterizes a rounded rectangle (or 0-corner rectangle) filled with the given DiamondGradient.
  // `left/top/width/height` describe the rectangle in its enclosing layer's coordinate space;
  // the gradient's center/matrix are assumed to be in that same space. `pixelRatio` controls the
  // output PNG's pixel density relative to CSS pixels (usually 2.0 for @2x output).
  // Returns true on success; false on any failure (no GL, surface creation, encode, file I/O).
  static bool RenderDiamondToPng(float left, float top, float width, float height, float roundness,
                                 const DiamondGradient* gradient, float pixelRatio,
                                 const std::string& outputPath);

  // Rasterizes an ellipse filled with the given DiamondGradient. Same coordinate conventions as
  // RenderDiamondToPng.
  static bool RenderDiamondEllipseToPng(float left, float top, float width, float height,
                                        const DiamondGradient* gradient, float pixelRatio,
                                        const std::string& outputPath);

  // Rasterizes a rounded rectangle (or 0-corner rectangle) filled with the given ImagePattern.
  // Useful when tileMode is Repeat/Mirror (CSS background-image handles Decal adequately but
  // cannot reproduce Mirror, and its Repeat semantics differ from tgfx's matrix-based tiling).
  static bool RenderImagePatternToPng(float left, float top, float width, float height,
                                      float roundness, const ImagePattern* pattern,
                                      float pixelRatio, const std::string& outputPath);

  // Rasterizes an ellipse filled with the given ImagePattern.
  static bool RenderImagePatternEllipseToPng(float left, float top, float width, float height,
                                             const ImagePattern* pattern, float pixelRatio,
                                             const std::string& outputPath);
};

}  // namespace pagx
