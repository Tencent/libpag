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

#include "tgfx/core/Data.h"
#include "tgfx/core/ImageInfo.h"
#include "tgfx/core/YUVColorSpace.h"
#include "tgfx/core/YUVData.h"
#include "tgfx/platform/HardwareBuffer.h"
#include "tgfx/platform/NativeImage.h"

namespace tgfx {
class Context;
class Texture;

/**
 * ImageBuffer describes a two-dimensional array of pixels which is optimized for creating textures.
 * ImageBuffer is immutable and safe across threads.
 */
class ImageBuffer {
 public:
  /**
   * Creates an ImageBuffer from the platform-specific hardware buffer. For example, the hardware
   * buffer could be an AHardwareBuffer on the android platform or a CVPixelBufferRef on the apple
   * platform. The returned ImageBuffer takes a reference to the hardwareBuffer. The caller must
   * ensure the buffer content stays unchanged for the lifetime of the returned ImageBuffer. The
   * colorSpace is ignored if the hardwareBuffer contains only one plane, which is not in the YUV
   * format. Returns nullptr if the hardwareBuffer is nullptr.
   */
  static std::shared_ptr<ImageBuffer> MakeFrom(
      HardwareBufferRef hardwareBuffer, YUVColorSpace colorSpace = YUVColorSpace::BT601_LIMITED);

  /**
   * Creates an ImageBuffer from the ImageInfo and shares pixels from the immutable Data object. The
   * pixel data may be copied and converted to a new format which is more efficient for texture
   * uploading. However, if the ImageInfo is suitable for direct texture uploading, the pixel data
   * will be shared instead of copied. In that case, the caller must ensure the pixel data stay
   * unchanged for the lifetime of the returned ImageBuffer. Returns nullptr if the info is empty or
   * the pixels are nullptr.
   * ImageInfo parameters suitable for direct texture uploading include:
   * The alpha type is not AlphaType::Unpremultiplied;
   * The color type is one of ColorType::ALPHA_8, ColorType::RGBA_8888, and ColorType::BGRA_8888.
   */
  static std::shared_ptr<ImageBuffer> MakeFrom(const ImageInfo& info, std::shared_ptr<Data> pixels);

  /**
   * Creates an ImageBuffer in the I420 format with the specified YUVData and YUVColorSpace. The
   * caller must ensure the yuvData stays unchanged for the lifetime of the returned ImageBuffer.
   * Returns nullptr if the yuvData is invalid.
   */
  static std::shared_ptr<ImageBuffer> MakeI420(
      std::shared_ptr<YUVData> yuvData, YUVColorSpace colorSpace = YUVColorSpace::BT601_LIMITED);

  /**
   * Creates an ImageBuffer in the NV12 format with the specified YUVData and YUVColorSpace. The
   * caller must ensure the yuvData stays unchanged for the lifetime of the returned ImageBuffer.
   * Returns nullptr if the yuvData is invalid.
   */
  static std::shared_ptr<ImageBuffer> MakeNV12(
      std::shared_ptr<YUVData> yuvData, YUVColorSpace colorSpace = YUVColorSpace::BT601_LIMITED);

  virtual ~ImageBuffer() = default;
  /**
   * Returns the width of the image buffer.
   */
  virtual int width() const = 0;

  /**
   * Returns the height of the image buffer.
   */
  virtual int height() const = 0;

  /**
   * Returns true if pixels represent transparency only. If true, each pixel is packed in 8 bits as
   * defined by ColorType::ALPHA_8.
   */
  virtual bool isAlphaOnly() const = 0;

 protected:
  ImageBuffer() = default;

  /**
   * Creates a new Texture capturing the pixels in this image buffer. The mipMapped parameter
   * specifies whether created texture must allocate mip map levels.
   */
  virtual std::shared_ptr<Texture> onMakeTexture(Context* context, bool mipMapped) const = 0;

  friend class Texture;
};
}  // namespace tgfx
