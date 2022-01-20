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

#include "ImageInfo.h"
#include "gpu/TextureBuffer.h"

namespace pag {
/**
 * PixelBuffer describes a two dimensional array of pixels located in CPU memory.
 */
class PixelBuffer : public TextureBuffer {
 public:
  /**
   * Creates a new PixelBuffer object width specified width and height. Returns nullptr if width or
   * height is not greater thant zero.
   * @param width pixel column count, must be greater than zero.
   * @param height pixel row count, must be greater than zero.
   * @param alphaOnly If true, sets colorType to ColorType::ALPHA_8, otherwise sets to the native
   * 32-bit color type of current platform.
   * @param tryHardware If true, a hardware PixelBuffer is returned if it is available on current
   * platform, otherwise a raster PixelBuffer is returned.
   */
  static std::shared_ptr<PixelBuffer> Make(int width, int height, bool alphaOnly = false,
                                           bool tryHardware = true);

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
   * Returns true if this pixel buffer is hardware backed.
   */
  bool isHardwareBacked() const {
    return hardwareBacked;
  }

  /**
   * Replaces all pixel values with transparent colors.
   */
  void eraseAll();

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

  explicit PixelBuffer(const ImageInfo& info)
      : TextureBuffer(info.width(), info.height()), _info(info) {
  }

 private:
  bool hardwareBacked = false;
};

/**
 * Trace() provides a utility to view the PixelBuffer.
 */
void Trace(std::shared_ptr<PixelBuffer> pixelBuffer, const std::string& tag = "");
}  // namespace pag