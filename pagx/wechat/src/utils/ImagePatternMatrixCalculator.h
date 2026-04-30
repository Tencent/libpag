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

#include <cstddef>
#include <string>

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
// origImageWidth/origImageHeight are used only by the TILE branch to compensate progressive
// image loading: they represent the full-resolution pixel dimensions recorded by the exporter
// so the tile period stays stable while imageWidth/imageHeight grow from placeholder toward
// full resolution. Pass 0 for both to disable compensation and fall back to cocraft's
// ratio = scaleFactor behavior.
pagx::Matrix calculateImagePatternMatrix(ImageScaleMode scaleMode, float imageWidth, float imageHeight,
                                         float nodeWidth, float nodeHeight, const pagx::Matrix& paintTransform,
                                         float scaleFactor = 0.5f,
                                         float origImageWidth = 0.0f, float origImageHeight = 0.0f);

// Resolves a single ImagePattern's transform matrix from its customData and actual image
// dimensions. Idempotent: the original paint transform is cached in customData on first call,
// so repeated invocations after decodedImage changes recompute against the current image size
// rather than re-baking the previously baked matrix.
bool resolveImagePatternMatrix(pagx::ImagePattern* pattern);

// Resolves all ImagePattern transform matrices in the document. Should be called after loading
// external image data and before LayerBuilder::Build().
void resolveAllImagePatternMatrices(pagx::PAGXDocument* document);

// Resolves ImagePattern transform matrices for every pattern whose backing Image node has the
// given filePath. Returns the number of patterns whose matrix was refreshed. Intended for the
// progressive image upgrade path: when a higher-resolution image replaces the initial one, the
// baked pattern matrix must be recomputed against the new image dimensions.
size_t resolveImagePatternMatricesByFilePath(pagx::PAGXDocument* document,
                                             const std::string& filePath);

}  // namespace pagx
