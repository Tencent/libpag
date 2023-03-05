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

#include <string>
#include "tgfx/core/Point.h"
#include "tgfx/gpu/Resource.h"
#include "tgfx/gpu/SurfaceOrigin.h"
#include "tgfx/gpu/TextureSampler.h"
#include "tgfx/platform/HardwareBuffer.h"

namespace tgfx {
/**
 * Texture describes a two dimensional array of pixels in the GPU backend for drawing.
 */
class Texture : public Resource {
 public:
  /**
   * Creates a single-plane texture from a platform-specific hardware buffer. For example, the
   * hardware buffer could be an AHardwareBuffer on the android platform or a CVPixelBufferRef on
   * the apple platform. The returned Texture takes a reference on the hardwareBuffer. Returns
   * nullptr if any of the parameters are nullptr or the hardwareBuffer is not single-plane. Use the
   * YUVTexture::MakeFrom() method for the hardware buffer with multiple planes.
   */
  static std::shared_ptr<Texture> MakeFrom(Context* context, HardwareBufferRef hardwareBuffer);

  /**
   * Creates a new texture from the specified pixel data with each pixel stored as 32-bit RGBA
   * data. Returns nullptr if any of the parameters is invalid.
   */
  static std::shared_ptr<Texture> MakeRGBA(Context* context, int width, int height,
                                           const void* pixels, size_t rowBytes,
                                           SurfaceOrigin origin = SurfaceOrigin::TopLeft,
                                           bool mipMapped = false) {
    return MakeFormat(context, width, height, pixels, rowBytes, PixelFormat::RGBA_8888, origin,
                      mipMapped);
  }
  /**
   * Creates a empty texture with each pixel stored as 32-bit RGBA data. Returns nullptr if any of
   * the parameters is invalid.
   */
  static std::shared_ptr<Texture> MakeRGBA(Context* context, int width, int height,
                                           SurfaceOrigin origin = SurfaceOrigin::TopLeft,
                                           bool mipMapped = false) {
    return MakeFormat(context, width, height, nullptr, 0, PixelFormat::RGBA_8888, origin,
                      mipMapped);
  }

  /**
   * Creates a new texture from the specified pixel data with each pixel stored as a single
   * translucency (alpha) channel. Returns nullptr if any of the parameters is invalid or the
   * backend does not support creating alpha only textures.
   */
  static std::shared_ptr<Texture> MakeAlpha(Context* context, int width, int height,
                                            const void* pixels, size_t rowBytes,
                                            SurfaceOrigin origin = SurfaceOrigin::TopLeft,
                                            bool mipMapped = false) {
    return MakeFormat(context, width, height, pixels, rowBytes, PixelFormat::ALPHA_8, origin,
                      mipMapped);
  }
  /**
   * Creates a empty texture with each pixel stored as a single translucency (alpha) channel.
   * Returns nullptr if any of the parameters is invalid or the backend does not support creating
   * alpha only textures.
   */
  static std::shared_ptr<Texture> MakeAlpha(Context* context, int width, int height,
                                            SurfaceOrigin origin = SurfaceOrigin::TopLeft,
                                            bool mipMapped = false) {
    return MakeFormat(context, width, height, nullptr, 0, PixelFormat::ALPHA_8, origin, mipMapped);
  }

  /**
   * Creates a new texture from the specified pixel data with each pixel store as the pixelFormat
   * describes. Returns nullptr if any of the parameters is invalid or the backend does not support
   * creating textures of the specified pixelFormat.
   */
  static std::shared_ptr<Texture> MakeFormat(Context* context, int width, int height,
                                             const void* pixels, size_t rowBytes,
                                             PixelFormat pixelFormat, SurfaceOrigin origin,
                                             bool mipMapped = false);

  /**
   * Creates a empty texture with each pixel store as the pixelFormat describes. Returns nullptr if
   * any of the parameters is invalid or the backend does not support creating textures of the
   * specified pixelFormat.
   */
  static std::shared_ptr<Texture> MakeFormat(Context* context, int width, int height,
                                             PixelFormat pixelFormat, SurfaceOrigin origin,
                                             bool mipMapped = false) {
    return MakeFormat(context, width, height, nullptr, 0, pixelFormat, origin, mipMapped);
  }

  Texture(int width, int height, SurfaceOrigin origin)
      : _width(width), _height(height), _origin(origin) {
  }

  /**
   * Returns the display width of this texture.
   */
  int width() const {
    return _width;
  }

  /**
   * Returns the display height of this texture.
   */
  int height() const {
    return _height;
  }

  /**
   * Returns the origin of this texture, either SurfaceOrigin::TopLeft or SurfaceOrigin::BottomLeft.
   */
  SurfaceOrigin origin() const {
    return _origin;
  }

  /**
   * Returns the texture coordinates in backend units corresponding to specified position in pixels.
   */
  virtual Point getTextureCoord(float x, float y) const = 0;

  /**
   * Returns the default texture sampler.
   */
  virtual const TextureSampler* getSampler() const = 0;

  /**
   * Returns true if this is a YUVTexture.
   */
  virtual bool isYUV() const {
    return false;
  }

 private:
  int _width = 0;
  int _height = 0;
  SurfaceOrigin _origin = SurfaceOrigin::TopLeft;

  /**
   * Returns true if the specified texture size and format are supported by the GPU backend.
   */
  static bool CheckSizeAndFormat(Context* context, int width, int height, PixelFormat format);

  friend class ProxyProvider;
};
}  // namespace tgfx
