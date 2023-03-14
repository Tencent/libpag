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

#include <CoreVideo/CoreVideo.h>
#include "gpu/Texture.h"
#include "gpu/TextureSampler.h"

namespace tgfx {
class CGLHardwareTexture : public Texture {
 public:
  static std::shared_ptr<CGLHardwareTexture> MakeFrom(Context* context,
                                                      CVPixelBufferRef pixelBuffer,
                                                      CVOpenGLTextureCacheRef textureCache);

  explicit CGLHardwareTexture(CVPixelBufferRef pixelBuffer);

  ~CGLHardwareTexture() override;

  size_t memoryUsage() const override;

  const TextureSampler* getSampler() const override {
    return sampler.get();
  }

 protected:
  void computeRecycleKey(BytesKey* recycleKey) const override;

 private:
  std::unique_ptr<TextureSampler> sampler = {};
  CVPixelBufferRef pixelBuffer = nullptr;
  CVOpenGLTextureRef texture = nil;
  CVOpenGLTextureCacheRef textureCache = nil;

  static void ComputeRecycleKey(BytesKey* recycleKey, CVPixelBufferRef pixelBuffer);

  void onReleaseGPU() override;
};
}  // namespace tgfx
