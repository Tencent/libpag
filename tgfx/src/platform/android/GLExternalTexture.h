/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "tgfx/gpu/opengl/GLTexture.h"

namespace tgfx {
class GLExternalTexture : public GLTexture {
 public:
  static std::shared_ptr<GLExternalTexture> Make(Context* context, int width, int height);

  tgfx::Point getTextureCoord(float x, float y) const override;

  size_t memoryUsage() const override;

  void updateTextureSize(int width, int height);

 private:
  int textureWidth = 0;
  int textureHeight = 0;

  GLExternalTexture(GLSampler sampler, int width, int height);

  void onReleaseGPU() override;
};
}  // namespace tgfx
