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

#include "GLContext.h"
#include "GLTextureSampler.h"
#include "gpu/Texture.h"

namespace pag {
class GLTexture : public Texture {
 public:
  static GLTextureInfo Unwrap(const Texture* texture);

  /**
   * Creates a new texture with each pixel stored as a single translucency (alpha) channel.
   */
  static std::shared_ptr<GLTexture> MakeAlpha(Context* context, int width, int height,
                                              ImageOrigin origin = ImageOrigin::TopLeft);

  /**
   * Creates a new texture with each pixel stored as 32-bit RGBA data.
   */
  static std::shared_ptr<GLTexture> MakeRGBA(Context* context, int width, int height,
                                             ImageOrigin origin = ImageOrigin::TopLeft);

  Point getTextureCoord(float x, float y) const override;

  const TextureSampler* getSampler() const override {
    return &sampler;
  }

  const GLTextureInfo& getGLInfo() const {
    return sampler.glInfo;
  }

 protected:
  GLTextureSampler sampler = {};

  GLTexture(int width, int height, ImageOrigin origin);

  friend class Texture;
};
}  // namespace pag
