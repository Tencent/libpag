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
#include "tgfx/core/ImageBuffer.h"
#include "tgfx/platform/HardwareBuffer.h"

namespace tgfx {
/**
 * A container for writable pixel memory. PixelBuffer is thread safe.
 */
class PixelBuffer : public ImageBuffer {
 public:
  /**
   * Creates a new PixelBuffer object width specified width and height. Returns nullptr if width or
   * height is not greater than zero. If the alphaOnly is true, sets colorType to
   * ColorType::ALPHA_8, otherwise sets to the native 32-bit color type of the current platform. If
   * the tryHardware is true, a PixelBuffer backed by hardware is returned if it is available on the
   * current platform. Otherwise, a raster PixelBuffer is returned.
   */
  static std::shared_ptr<PixelBuffer> Make(int width, int height, bool alphaOnly = false,
                                           bool tryHardware = true);

  /**
   * Creates a PixelBuffer from the specified hardware buffer. Returns nullptr if the hardwareBuffer
   * is invalid or the current platform has no hardware buffer support.
   */
  static std::shared_ptr<PixelBuffer> MakeFrom(HardwareBufferRef hardwareBuffer);

  int width() const override {
    return _info.width();
  }

  int height() const override {
    return _info.height();
  }

  bool isAlphaOnly() const override {
    return _info.isAlphaOnly();
  }

  /**
   * Returns an ImageInfo describing the width, height, color type, alpha type, and row bytes of the
   * PixelBuffer.
   */
  const ImageInfo& info() const {
    return _info;
  }

  /**
   * Locks and returns the address of the pixels to ensure that the memory is accessible.
   */
  void* lockPixels();

  /**
   * Call this to balance a successful call to lockPixels().
   */
  void unlockPixels();

  /**
   * Returns true if this pixel buffer is hardware backed. A hardware-backed PixelBuffer allows
   * sharing buffers across CPU and GPU, which can be used to speed up the texture uploading.
   */
  virtual bool isHardwareBacked() const = 0;

  /**
   * Retrieves the backing hardware buffer. This method does not acquire any additional reference to
   * the returned hardware buffer. Returns nullptr if the PixelBuffer is not backed by a hardware
   * buffer.
   */
  virtual HardwareBufferRef getHardwareBuffer() const = 0;

 protected:
  explicit PixelBuffer(const ImageInfo& info);

  std::shared_ptr<Texture> onMakeTexture(Context* context, bool mipMapped) const override;

  virtual void* onLockPixels() const = 0;
  virtual void onUnlockPixels() const = 0;
  virtual std::shared_ptr<Texture> onBindToHardwareTexture(Context* context) const = 0;

 private:
  mutable std::mutex locker = {};
  ImageInfo _info = {};
};
}  // namespace tgfx