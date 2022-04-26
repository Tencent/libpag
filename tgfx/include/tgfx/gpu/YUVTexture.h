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

#include "Texture.h"
#include "tgfx/core/YUVInfo.h"

namespace tgfx {

/**
 * YUVTexture wraps separate texture units in the GPU backend for Y, U, and V planes.
 */
class YUVTexture : public Texture {
 public:
  /**
   * Creates a new YUV texture from I420 buffers.
   */
  static std::shared_ptr<YUVTexture> MakeI420(Context* context, YUVColorSpace colorSpace,
                                              YUVColorRange colorRange, int width, int height,
                                              uint8_t* pixelsPlane[3], const int lineSize[3]);

  /**
   * Creates a new YUV texture from NV12 buffers.
   */
  static std::shared_ptr<YUVTexture> MakeNV12(Context* context, YUVColorSpace colorSpace,
                                              YUVColorRange colorRange, int width, int height,
                                              uint8_t* pixelsPlane[2], const int lineSize[2]);

  /**
   * Creates a new YUV texture from a hardware buffer. The type of hardwareBuffer should be
   * either AHardwareBuffer* on android platform or CVPixelBufferRef on apple platform. The returned
   * Texture takes a reference on the buffer.
   */
  static std::shared_ptr<YUVTexture> MakeFrom(Context* context, YUVColorSpace colorSpace,
                                              YUVColorRange colorRange, void* hardwareBuffer);

  YUVTexture(YUVColorSpace colorSpace, YUVColorRange colorRange, int width, int height)
      : Texture(width, height, ImageOrigin::TopLeft),
        _colorSpace(colorSpace),
        _colorRange(colorRange) {
  }

  /**
   * The pixel format of this yuv texture.
   */
  virtual YUVPixelFormat pixelFormat() const = 0;

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
   * The color space of this yuv texture.
   */
  YUVColorSpace colorSpace() const {
    return _colorSpace;
  }

  /**
   * The color range of this yuv texture.
   */
  YUVColorRange colorRange() const {
    return _colorRange;
  }

  bool isYUV() const final {
    return true;
  }

 private:
  YUVColorSpace _colorSpace = YUVColorSpace::Rec601;
  YUVColorRange _colorRange = YUVColorRange::MPEG;
};
}  // namespace tgfx
