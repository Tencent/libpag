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
#include <unordered_map>
#include <utility>

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
pagx::Matrix CalculateImagePatternMatrix(ImageScaleMode scaleMode, float imageWidth, float imageHeight,
                                         float nodeWidth, float nodeHeight, const pagx::Matrix& paintTransform,
                                         float scaleFactor = 0.5f,
                                         float origImageWidth = 0.0f, float origImageHeight = 0.0f);

// Per-image original-pixel-size lookup used by the new-format resolve path. Maps the Image
// node's filePath ("hash:..." / "emoji:...") to the full-resolution pixel dimensions that the
// exporter assumed when baking pattern->matrix. The table is owned by PAGXView and populated
// via PAGXView::setImageOriginalSize() before attachNativeImage(); ResolveImagePatternMatrix()
// reads it to undo any scaling applied by progressive thumbnail/full downloads. May be null
// to skip the new-format path entirely (legacy customData-only flow).
using ImageOriginalSizeMap = std::unordered_map<std::string, std::pair<float, float>>;

// Resolves a single ImagePattern's transform matrix from its customData and actual image
// dimensions. Idempotent: the original paint transform / matrix is cached in customData on
// first call, so repeated invocations after provider state changes recompute against the
// current image size rather than re-baking the previously baked matrix.
//
// Two formats are supported:
//   1. Legacy CoCraft format (customData carries image-scale-mode, node-width/height,
//      orig-image-width/height, scale-factor): rebuilt via CalculateImagePatternMatrix() against
//      the current image's pixel dimensions.
//   2. New PAGX standard format (no image-scale-mode in customData): the exporter has already
//      baked pattern->matrix in original-image pixel coordinates. When the actually attached
//      image has different pixel dimensions (progressive thumbnail / CDN-shrunk variant), the
//      matrix is post-scaled by diag(origW/actualW, origH/actualH) so tgfx's getFitMatrix /
//      shader sampling sees the same visual layout regardless of the asset's actual resolution.
//      origW/origH source priority: first read from pattern->customData
//      ("orig-image-width"/"orig-image-height", written by the new exporter); if missing,
//      fall back to origSizeMap[filePath] populated by the host via setImageOriginalSize();
//      if both are unavailable the authored matrix is left unchanged.
bool ResolveImagePatternMatrix(pagx::ImagePattern* pattern,
                               const ImageOriginalSizeMap* origSizeMap = nullptr,
                               pagx::ImageResourceProvider* provider = nullptr);

// Resolves all ImagePattern transform matrices in the document. Should be called after loading
// external image data and before LayerBuilder::Build().
void ResolveAllImagePatternMatrices(pagx::PAGXDocument* document,
                                    const ImageOriginalSizeMap* origSizeMap = nullptr);

// Resolves ImagePattern transform matrices for every pattern whose backing Image node has the
// given filePath. Returns the number of patterns whose matrix was refreshed. Intended for the
// progressive image upgrade path: when a higher-resolution image replaces the initial one, the
// baked pattern matrix must be recomputed against the new image dimensions.
size_t ResolveImagePatternMatricesByFilePath(pagx::PAGXDocument* document,
                                             const std::string& filePath,
                                             const ImageOriginalSizeMap* origSizeMap = nullptr);

}  // namespace pagx
