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
#include "gpu/YUVTexture.h"

namespace tgfx {
struct YUVConfig;
class GLYUVTexture : public YUVTexture {
 public:
  Point getTextureCoord(float x, float y) const override;

  size_t samplerCount() const override {
    return samplers.size();
  }

  const TextureSampler* getSamplerAt(size_t index) const override;

  size_t planeCount() const {
    return samplerCount();
  }

 protected:
  std::vector<GLTextureSampler> samplers = {};

  GLYUVTexture(YUVColorSpace colorSpace, YUVColorRange colorRange, int width, int height);

  void onRelease(Context*) override;

  friend class YUVTexture;
};
}  // namespace tgfx
