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
#include "pagx/nodes/ImagePattern.h"
#include "pagx/types/Matrix.h"

namespace pagx {

// Image scale modes corresponding to CoCraft Schema::ImageScaleMode enum values.
enum class ImageScaleMode : int {
  STRETCH = 0,
  FIT = 1,
  FILL = 2,
  TILE = 3,
};

// Calculates the ImagePattern transform matrix based on the image scale mode.
pagx::Matrix CalculateImagePatternMatrix(ImageScaleMode scaleMode, float imageWidth,
                                         float imageHeight, float nodeWidth, float nodeHeight,
                                         const pagx::Matrix& paintTransform,
                                         float scaleFactor = 0.5f);

// Resolves a single ImagePattern's transform matrix from its customData and actual image dimensions.
bool ResolveImagePatternMatrix(pagx::ImagePattern* pattern);

// Resolves all ImagePattern transform matrices in the document. Should be called after loading
// external image data and before LayerBuilder::Build().
void ResolveAllImagePatternMatrices(pagx::PAGXDocument* document);

// Forces all Gradient nodes in the document to use absolute coordinates
// (fitsToGeometry = false). Should be called before LayerBuilder::Build() until the pagx exporter
// adapts to relative coordinates for gradient fills.
void ResolveAllGradientCoordinates(pagx::PAGXDocument* document);

}  // namespace pagx
