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

#include "tgfx/core/ImageBuffer.h"

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
   * Creates a hardware-backed PixelBuffer with the specified width and height. Returns nullptr if
   * the current platform has no hardware buffer support. Hardware buffer is a low-level object
   * representing a memory buffer accessible by various hardware units. Hardware buffer allows
   * sharing buffers across CPU and GPU, which can be used to speed up the texture uploading.
   */
  static std::shared_ptr<PixelBuffer> MakeHardwareBuffer(int width, int height, bool alphaOnly);

  int width() const override {
    return _info.width();
  }

  int height() const override {
    return _info.height();
  }

  bool isAlphaOnly() const override {
    return _info.colorType() == ColorType::ALPHA_8;
  }

  /**
   * Returns an ImageInfo describing the width, height, color type, alpha type, and row bytes of the
   * PixelBuffer.
   */
  const ImageInfo& info() const {
    return _info;
  }

  /**
   * Returns true if this pixel buffer is hardware backed. A hardware backed PixelBuffer allows
   * sharing buffers across CPU and GPU, which can be used to speed up the texture uploading.
   */
  virtual bool isHardwareBacked() const = 0;

  /**
   * Locks and returns the address of the pixels to ensure that the memory is accessible.
   */
  virtual void* lockPixels() = 0;

  /**
   * Call this to balance a successful call to lockPixels().
   */
  virtual void unlockPixels() = 0;

 protected:
  const ImageInfo _info = {};

  explicit PixelBuffer(const ImageInfo& info) : _info(info) {
  }
};
}  // namespace tgfx