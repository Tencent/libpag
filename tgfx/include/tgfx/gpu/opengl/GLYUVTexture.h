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

#include "tgfx/gpu/YUVTexture.h"
#include "tgfx/gpu/opengl/GLSampler.h"

namespace tgfx {
/**
 * GLYUVTexture wraps separate texture units in the OpenGL backend for Y, U, and V planes.
 */
class GLYUVTexture : public YUVTexture {
 public:
  Point getTextureCoord(float x, float y) const override;

  size_t samplerCount() const override {
    return samplers.size();
  }

  const TextureSampler* getSamplerAt(size_t index) const override;

 protected:
  std::vector<GLSampler> samplers = {};

  GLYUVTexture(YUVColorSpace colorSpace, YUVColorRange colorRange, int width, int height);

 private:
  void onReleaseGPU() override;

  friend class YUVTexture;
};
}  // namespace tgfx
