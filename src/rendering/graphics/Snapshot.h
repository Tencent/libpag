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

#pragma once

#include "pag/types.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/Matrix.h"

namespace pag {
class RenderCache;

/**
 * A Snapshot contains a image cache for specified Graphic, suitable for immediate drawing on the
 * GPU.
 */
class Snapshot {
 public:
  /**
   * Creates a new Snapshot with the specified image and matrix.
   */
  explicit Snapshot(std::shared_ptr<tgfx::Image> image, const tgfx::Matrix& matrix)
      : image(std::move(image)), matrix(matrix) {
  }

  /**
   * Returns the scaling factor of this snapshot to the original graphic content.
   */
  float scaleFactor() const {
    return 1 / matrix.getScaleX();
  }

  tgfx::Matrix getMatrix() const {
    return matrix;
  }

  std::shared_ptr<tgfx::Image> getImage() const {
    return image;
  }

  /**
   * Returns memory usage information for this Snapshot.
   */
  size_t memoryUsage() const;

  /**
   * Evaluates the Snapshot to see if it overlaps or intersects with the specified point. The point
   * is in the coordinate space of the Snapshot. This method always checks against the actual pixels
   * of the Snapshot.
   */
  bool hitTest(RenderCache* cache, float x, float y) const;

 private:
  std::shared_ptr<tgfx::Image> image = nullptr;
  tgfx::Matrix matrix = tgfx::Matrix::I();
  ID assetID = 0;
  uint64_t makerKey = 0;
  Frame idleFrames = 0;

  friend class RenderCache;
};
}  // namespace pag
