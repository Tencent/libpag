/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "SurfaceUtil.h"
#include "base/utils/MatrixUtil.h"

namespace pag {
// 1/20 is the minimum precision for rendering pixels on most platforms.
#define CONTENT_SCALE_STEP 20.0f

std::shared_ptr<tgfx::Surface> SurfaceUtil::MakeContentSurface(Canvas* parentCanvas,
                                                               const tgfx::Rect& bounds,
                                                               float scaleFactorLimit, float scale,
                                                               bool usesMSAA) {
  auto maxScale = GetMaxScaleFactor(parentCanvas->getMatrix());
  maxScale *= scale;
  if (maxScale > scaleFactorLimit) {
    maxScale = scaleFactorLimit;
  } else {
    // Snap the scale value to 1/20 to prevent edge shaking when rendering zoom-in animations.
    maxScale = ceilf(maxScale * CONTENT_SCALE_STEP) / CONTENT_SCALE_STEP;
  }
  auto width = static_cast<int>(ceilf(bounds.width() * maxScale));
  auto height = static_cast<int>(ceil(bounds.height() * maxScale));
  // LOGE("makeContentSurface: (width = %d, height = %d)", width, height);
  auto sampleCount = usesMSAA ? 4 : 1;
  auto newSurface =
      tgfx::Surface::Make(parentCanvas->getContext(), width, height, false, sampleCount);
  if (newSurface == nullptr) {
    return nullptr;
  }
  auto newCanvas = newSurface->getCanvas();
  auto matrix = tgfx::Matrix::MakeScale(maxScale);
  matrix.preTranslate(-bounds.x(), -bounds.y());
  newCanvas->setMatrix(matrix);
  return newSurface;
}
}  // namespace pag
