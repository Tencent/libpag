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

namespace pag {
/**
 * PixelMap is a low level class which provides convenience functions to access raster destinations,
 * which provides a utility to convert the format of pixels from one to another or compress the
 * pixels into a image file. PixelMap does not try to manage the lifetime of the pixel memory.
 */
class PixelMap {
 public:
  /**
   * Creates a new PixelMap with specified ImageInfo and pixels.
   */
  PixelMap(const ImageInfo& info, const void* pixels) : _info(info), _pixels(pixels) {
  }

  /**
   * Return true if this PixelMap describes an empty area of pixels.
   */
  bool isEmpty() const {
    return _info.isEmpty() || _pixels == nullptr;
  }

  /**
   * Returns a ImageInfo describing the width, height, color type, alpha type, and row bytes of the
   * pixels.
   */
  const ImageInfo& info() const {
    return _info;
  }

  /**
   * Returns the width of the pixels.
   */
  int width() const {
    return _info.width();
  }

  /**
   * Returns the height of the pixels.
   */
  int height() const {
    return _info.height();
  }

  /**
   * Returns the ColorType of the pixels.
   */
  ColorType colorType() const {
    return _info.colorType();
  }

  /**
   * Returns the AlphaType of the pixels.
   */
  AlphaType alphaType() const {
    return _info.alphaType();
  }

  /**
   * Returns the rowBytes of the pixels.
   */
  size_t rowBytes() const {
    return _info.rowBytes();
  }

  /**
   * Returns the byte size of the pixels.
   */
  size_t byteSize() const {
    return _info.byteSize();
  }

  /**
   * Returns pixel address, the base address corresponding to the pixel origin.
   */
  const void* pixels() const {
    return _pixels;
  }

  /**
   * Copies a rect of pixels to dstPixels with specified ImageInfo. Copy starts at (srcX, srcY), and
   * does not exceed PixelMap (width(), height()). Pixels are copied only if pixel conversion is
   * possible. Returns true if pixels are copied to dstPixels.
   */
  bool readPixels(const ImageInfo& dstInfo, void* dstPixels, int srcX = 0, int srcY = 0) const;

 private:
  ImageInfo _info = {};
  const void* _pixels = nullptr;
};

/**
 * Trace() provides a utility to view the PixelMap.
 */
void Trace(const PixelMap& pixelMap, const std::string& tag = "");
}  // namespace pag