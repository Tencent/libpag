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
#include "tgfx/core/ImageInfo.h"
#include "tgfx/platform/HardwareBuffer.h"

namespace tgfx {
/**
 * A container for writable pixel memory.
 */
class PixelBuffer : public ImageBuffer {
 public:
  /**
   * Creates a new PixelBuffer object width specified width and height. Returns nullptr if width or
   * height is not greater thant zero.
   * @param width pixel column count, must be greater than zero.
   * @param height pixel row count, must be greater than zero.
   * @param alphaOnly If true, sets colorType to ColorType::ALPHA_8, otherwise sets to the native
   * 32-bit color type of current platform.
   * @param tryHardware If true, a PixelBuffer backed by hardware is returned if it is available on
   * current platform. Otherwise, a raster PixelBuffer is returned.
   */
  static std::shared_ptr<PixelBuffer> Make(int width, int height, bool alphaOnly = false,
                                           bool tryHardware = true);

  /**
   * Creates a new PixelBuffer object from a hardware buffer. The hardware buffer could be an
   * AHardwareBuffer on the android platform or a CVPixelBufferRef on the apple platform. Returns
   * nullptr if the current platform has no hardware buffer support. The returned PixelBuffer takes
   * a reference on the buffer.
   */
  static std::shared_ptr<PixelBuffer> MakeFrom(HardwareBufferRef hardwareBuffer);

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
 * Returns a ImageInfo describing the width, height, color type, alpha type, and row bytes
  of the PixelMap.
  */
  const ImageInfo& info() const {
    return _info;
  }

  /**
   * Returns the AlphaType of the pixel buffer, which is always AlphaType::Premultiplied.
   */
  AlphaType alphaType() const {
    return _info.alphaType();
  }

  /**
   * Returns the ColorType of the pixel buffer.
   */
  ColorType colorType() const {
    return _info.colorType();
  }

  /**
   * Returns the rowBytes of the pixel buffer.
   */
  size_t rowBytes() const {
    return _info.rowBytes();
  }

  /**
   * Returns the byte size of the pixel buffer.
   */
  size_t byteSize() const {
    return _info.byteSize();
  }

  /**
   * Returns true if this pixel buffer is hardware backed. A hardware backed PixelBuffer allows
   * sharing buffers across CPU and GPU, which can be used to speed up the texture uploading.
   */
  bool isHardwareBacked() const {
    return hardwareBacked;
  }

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

 private:
  bool hardwareBacked = false;

  /**
   * Creates a hardware backed PixelBuffer with specified width and height. Returns nullptr if
   * current platform has no hardware buffer support. Hardware buffer is a low-level object
   * representing a memory buffer accessible by various hardware units. Hardware buffer allows
   * sharing buffers across CPU and GPU, which can be used to speed up the texture uploading.
   */
  static std::shared_ptr<PixelBuffer> MakeHardwareBuffer(int width, int height, bool alphaOnly);
};
}  // namespace tgfx