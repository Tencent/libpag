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

#include "tgfx/core/YUVColorSpace.h"
#include "tgfx/core/YUVData.h"
#include "tgfx/gpu/Texture.h"
#include "tgfx/gpu/YUVPixelFormat.h"

namespace tgfx {
/**
 * YUVTexture wraps separate texture units in the GPU backend for Y, U, and V planes.
 */
class YUVTexture : public Texture {
 public:
  /**
   * Creates a new YUV texture in the I420 format from the specified YUVData.
   */
  static std::shared_ptr<YUVTexture> MakeI420(
      Context* context, const YUVData* yuvData,
      YUVColorSpace colorSpace = YUVColorSpace::BT601_LIMITED);

  /**
   * Creates a new YUV texture in the NV12 format from the specified YUVData.
   */
  static std::shared_ptr<YUVTexture> MakeNV12(
      Context* context, const YUVData* yuvData,
      YUVColorSpace colorSpace = YUVColorSpace::BT601_LIMITED);

  /**
   * Creates a new YUV texture from a platform-specific hardware buffer. For example, the hardware
   * buffer could be an AHardwareBuffer on the android platform or a CVPixelBufferRef on the apple
   * platform. The returned YUVTexture takes a reference to the hardwareBuffer.
   */
  static std::shared_ptr<YUVTexture> MakeFrom(
      Context* context, HardwareBufferRef hardwareBuffer,
      YUVColorSpace colorSpace = YUVColorSpace::BT601_LIMITED);

  /**
   * Returns the texture sampler at index 0.
   */
  const TextureSampler* getSampler() const override {
    return getSamplerAt(0);
  }

  /**
   * Returns the number of the samplers in this texture.
   */
  virtual size_t samplerCount() const = 0;

  /**
   * Returns a texture sampler at the specified index.
   */
  virtual const TextureSampler* getSamplerAt(size_t index) const = 0;

  /**
   * The pixel format of this yuv texture.
   */
  virtual YUVPixelFormat pixelFormat() const = 0;

  /**
   * The color space of the yuv texture.
   */
  YUVColorSpace colorSpace() const {
    return _colorSpace;
  }

  bool isYUV() const final {
    return true;
  }

 protected:
  YUVTexture(int width, int height, YUVColorSpace colorSpace)
      : Texture(width, height, SurfaceOrigin::TopLeft), _colorSpace(colorSpace) {
  }

 private:
  YUVColorSpace _colorSpace = YUVColorSpace::BT601_LIMITED;
};
}  // namespace tgfx
