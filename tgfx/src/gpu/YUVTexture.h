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
#include "gpu/TextureSampler.h"
#include "tgfx/core/YUVColorSpace.h"
#include "tgfx/core/YUVData.h"

namespace tgfx {
/**
 * Defines pixel formats for YUV textures.
 */
enum class YUVPixelFormat {
  /**
   * 8-bit Y plane followed by 8 bit 2x2 subsampled U and V planes.
   */
  I420,
  /**
   * 8-bit Y plane followed by an interleaved U/V plane with 2x2 subsampling.
   */
  NV12
};

/**
 * YUVTexture wraps separate texture samplers in the GPU backend for Y, U, and V planes.
 */
class YUVTexture : public Texture {
 public:
  /**
   * The pixel format of this yuv texture.
   */
  YUVPixelFormat pixelFormat() const {
    return _pixelFormat;
  }

  /**
   * The color space of the yuv texture.
   */
  YUVColorSpace colorSpace() const {
    return _colorSpace;
  }

  /**
   * Returns the number of the samplers in the texture.
   */
  size_t samplerCount() const {
    return samplers.size();
  }

  const TextureSampler* getSampler() const override {
    return samplers[0].get();
  }

  /**
   * Returns a texture sampler at the specified index.
   */
  const TextureSampler* getSamplerAt(size_t index) const;

  bool isYUV() const final {
    return true;
  }

  size_t memoryUsage() const override;

  Point getTextureCoord(float x, float y) const override;

  BackendTexture getBackendTexture() const override;

 protected:
  std::vector<std::unique_ptr<TextureSampler>> samplers = {};

  YUVTexture(int width, int height, YUVPixelFormat pixelFormat, YUVColorSpace colorSpace);

  void computeRecycleKey(BytesKey* recycleKey) const override;

 private:
  YUVPixelFormat _pixelFormat = YUVPixelFormat::I420;
  YUVColorSpace _colorSpace = YUVColorSpace::BT601_LIMITED;

  void onReleaseGPU() override;

  friend class Texture;
};
}  // namespace tgfx
