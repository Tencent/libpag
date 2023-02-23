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

#include "tgfx/core/YUVData.h"
#include "tgfx/gpu/Texture.h"
#include "tgfx/platform/HardwareBuffer.h"
#include "tgfx/platform/NativeImage.h"

namespace tgfx {
/**
 * ImageBuffer describes a two dimensional array of pixels which is optimized for creating textures.
 * ImageBuffer is immutable and safe across threads.
 */
class ImageBuffer {
 public:
  /**
   * Creates a single-plane ImageBuffer from a platform-specific hardware buffer. The hardwareBuffer
   * could be an AHardwareBuffer on the android platform or a CVPixelBufferRef on the apple
   * platform. The returned ImageBuffer takes a reference on the hardwareBuffer. The caller must
   * ensure that pixels are unchanged for the lifetime of the returned ImageBuffer. Returns nullptr
   * if any of the parameters is nullptr or the hardwareBuffer is not single-plane. Use the
   * YUVBuffer::MakeFrom() method for the hardware buffer with multiple planes.
   */
  static std::shared_ptr<ImageBuffer> MakeFrom(HardwareBufferRef hardwareBuffer);

  /**
   * Creates a new ImageBuffer object from a platform-specific image in the CPU. The nativeImage
   * could be a jobject that represents a java Bitmap on the android platform or a CGImageRef on the
   * apple platform.The returned ImageBuffer object takes a reference on the nativeImage. The caller
   * must ensure that pixels are unchanged for the lifetime of the returned ImageBuffer. Returns
   * nullptr if the nativeImage is nullptr or the current platform has no native image support.
   */
  static std::shared_ptr<ImageBuffer> MakeFrom(NativeImageRef nativeImage);

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

  /**
   * Returns true if the image buffer is a YUVBuffer.
   */
  virtual bool isYUV() const {
    return false;
  }

  /**
   * Creates a new Texture capturing the pixels in this image buffer. The optional mipMapped
   * parameter specifies whether created texture must allocate mip map levels.
   */
  std::shared_ptr<Texture> makeTexture(Context* context, bool mipMapped = false) const {
    return onMakeTexture(context, mipMapped);
  }

 protected:
  ImageBuffer() = default;

  virtual std::shared_ptr<Texture> onMakeTexture(Context* context, bool mipMapped) const = 0;
};
}  // namespace tgfx
