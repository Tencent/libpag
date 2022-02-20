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

#include "gpu/Texture.h"
#include "gpu/opengl/GLSampler.h"

namespace tgfx {
/**
 * Describes a two dimensional array of pixels in the OpenGL backend for drawing.
 */
class GLTexture : public Texture {
 public:
  Point getTextureCoord(float x, float y) const override;

  const TextureSampler* getSampler() const override {
    return &sampler;
  }

  /**
   * Returns the GLSampler associated with the texture.
   */
  const GLSampler& glSampler() const {
    return sampler;
  }

 protected:
  GLSampler sampler = {};

  GLTexture(int width, int height, ImageOrigin origin);

  friend class Texture;
};
}  // namespace tgfx
