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

#include <array>
#include "tgfx/core/Matrix.h"
#include "tgfx/gpu/opengl/GLFrameBuffer.h"
#include "tgfx/gpu/opengl/GLSampler.h"

namespace pag {
struct FilterSource {
  /**
   * The source texture sampler.
   */
  tgfx::GLSampler sampler = {};

  /**
   * The width of source texture in pixels after textureMatrix and scale being applied.
   */
  int width = 0;

  /**
   * The height of source texture in pixels after textureMatrix and scale being applied.
   */
  int height = 0;

  /**
   * Represent the scale factor of texture to the corresponding layer content. Filters need the
   * scale factors to interpret scalar parameters representing pixel distances in the image.For
   * example, a blur of 4 pixels should be interpreted as a blur of 2 pixels if the scale factor is
   * 1/2 in each direction.
   */
  tgfx::Point scale = {};

  /**
   * The 3x3 texture coordinate transform matrix. This transform matrix maps 2D texture coordinates
   * of the form (s, t, 0, 1) with s and t in the inclusive range [0, 1] to the texture coordinate
   * that should be used to sample that location from the texture. Sampling the texture outside of
   * the range of this transform is undefined. The matrix is stored in column-major order so that it
   * may be passed directly to OpenGL ES via the glLoadMatrixf or glUniformMatrix4fv functions.
   */
  std::array<float, 9> textureMatrix = {};
};

struct FilterTarget {
  /**
   * The target frame buffer.
   */
  tgfx::GLFrameBuffer frameBuffer = {};

  /**
   * The width of target frame buffer in pixels.
   */
  int width = 0;

  /**
   * The height of target frame buffer in pixels.
   */
  int height = 0;

  /**
   * The 3x3 vertex coordinate transform matrix. The matrix is stored in column-major order so that
   * it may be passed directly to OpenGL ES via the glLoadMatrixf or glUniformMatrix4fv functions.
   */
  std::array<float, 9> vertexMatrix = {};
};

class Filter {
 public:
  virtual ~Filter() = default;

  virtual bool initialize(tgfx::Context* context) = 0;

  /**
   * Apply this filter to a filter source and draw it to a filter target.
   */
  virtual void draw(tgfx::Context* context, const FilterSource* source,
                    const FilterTarget* target) = 0;

  virtual bool needsMSAA() const {
    return false;
  }
};
}  // namespace pag
