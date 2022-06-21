/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "tgfx/core/Matrix.h"
#include "tgfx/core/Mesh.h"
#include "tgfx/core/Path.h"
#include "tgfx/gpu/Texture.h"

namespace pag {
class RenderCache;

/**
 * A Snapshot contains a image cache for specified Graphic, suitable for immediate drawing on the
 * GPU.
 */
class Snapshot {
 public:
  /**
   * Creates a new Snapshot with specified texture and matrix.
   */
  explicit Snapshot(std::shared_ptr<tgfx::Texture> texture, const tgfx::Matrix& matrix)
      : texture(std::move(texture)), matrix(matrix) {
  }

  Snapshot(std::unique_ptr<tgfx::Mesh> mesh, const tgfx::Matrix& matrix)
      : matrix(matrix), mesh(std::move(mesh)) {
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

  std::shared_ptr<tgfx::Texture> getTexture() const {
    return texture;
  }

  const tgfx::Mesh* getMesh() const {
    return mesh.get();
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
  std::shared_ptr<tgfx::Texture> texture = nullptr;
  tgfx::Matrix matrix = tgfx::Matrix::I();
  ID assetID = 0;
  uint64_t makerKey = 0;
  tgfx::Path path = {};
  Frame idleFrames = 0;
  std::unique_ptr<tgfx::Mesh> mesh;

  friend class RenderCache;
};
}  // namespace pag
