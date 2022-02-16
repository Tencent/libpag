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

#include <algorithm>
#include <cinttypes>
#include <cstddef>

namespace tgfx {
/**
 * Describes how to interpret the alpha component of a pixel.
 */
enum class AlphaType {
  /**
   * uninitialized.
   */
  Unknown,
  /**
   * pixel is opaque.
   */
  Opaque,
  /**
   * pixel components are premultiplied by alpha.
   */
  Premultiplied,
  /**
   * pixel components are independent of alpha.
   */
  Unpremultiplied,
};

/**
 * Describes how pixel bits encode color. These values match up with the enum in Bitmap.Config on
 * Android platform.
 */
enum class ColorType {
  /**
   * uninitialized.
   */
  Unknown,
  /**
   * Each pixel is stored as a single translucency (alpha) channel. This is very useful to
   * efficiently store masks for instance. No color information is stored. With this configuration,
   * each pixel requires 1 byte of memory.
   */
  ALPHA_8,
  /**
   * Each pixel is stored on 4 bytes. Each channel (RGB and alpha for translucency) is stored with 8
   * bits of precision (256 possible values). The channel order is: red, green, blue, alpha.
   */
  RGBA_8888,
  /**
   * Each pixel is stored on 4 bytes. Each channel (RGB and alpha for translucency) is stored with 8
   * bits of precision (256 possible values). The channel order is: blue, green, red, alpha.
   */
  BGRA_8888,
};

/**
 * ImageInfo describes the properties for an area of pixels.
 */
class ImageInfo {
 public:
  /**
   * Returns true if the specified width and height is a valid size for pixels.
   */
  static bool IsValidSize(int width, int height);

  /**
   * Creates a new ImageInfo. Parameters are validated to see if their values are legal, or that the
   * combination is supported. Returns an empty ImageInfo if validating fails.
   */
  static ImageInfo Make(int width, int height, ColorType colorType,
                        AlphaType alphaType = AlphaType::Premultiplied, size_t rowBytes = 0);

  static int GetBytesPerPixel(ColorType colorType);

  /**
   * Creates an empty ImageInfo.
   */
  ImageInfo() = default;

  /**
   * Returns true if ImageInfo describes an empty area of pixels.
   */
  bool isEmpty() const {
    return _width <= 0;
  }

  /**
   * Returns the width of the pixels.
   */
  int width() const {
    return _width;
  }

  /**
   * Returns the height of the pixels.
   */
  int height() const {
    return _height;
  }

  /**
   * Returns the ColorType of the pixels.
   */
  ColorType colorType() const {
    return _colorType;
  }

  /**
   * Returns the AlphaType of the pixels.
   */
  AlphaType alphaType() const {
    return _alphaType;
  }

  /**
   * Returns the rowBytes of the pixels.
   */
  size_t rowBytes() const {
    return _rowBytes;
  }

  /**
   * Returns minimum bytes per row, computed from the width and colorType.
   */
  size_t minRowBytes() const {
    return _width * bytesPerPixel();
  }

  /**
   * Returns the byte size of the pixels, computed from the rowBytes and width.
   */
  size_t byteSize() const {
    return _rowBytes * _height;
  }

  /**
   * Returns number of bytes per pixel required by the colorType.
   */
  int bytesPerPixel() const;

  /**
   * Creates a new ImageInfo with dimensions set to width and height, and keep other properties the
   * same.
   */
  ImageInfo makeWH(int newWidth, int newHeight) const {
    return Make(newWidth, newHeight, _colorType, _alphaType, _rowBytes);
  }

  /**
   * If (0, 0, width(), height()) intersects (x, y, targetWidth, targetHeight), returns a new
   * ImageInfo with dimensions set to the size of intersection, and keep other properties the same.
   * Otherwise, returns a empty ImageInfo.
   */
  ImageInfo makeIntersect(int x, int y, int targetWidth, int targetHeight) const;

  /**
   * Creates a new ImageInfo with alphaType set to newAlphaType, and keep other properties the same.
   */
  ImageInfo makeAlphaType(AlphaType newAlphaType) const {
    return Make(_width, _height, _colorType, newAlphaType, _rowBytes);
  }

  /**
   * Creates ImageInfo with colorType set to newColorType, and keep other properties the same.
   */
  ImageInfo makeColorType(ColorType newColorType) const {
    return Make(_width, _height, newColorType, _alphaType, _rowBytes);
  }

  /**
   * Returns readable pixel address at (x, y) of specified base pixel address.
   * Note: The x value will be clamped to the range of (0, width), and the y value will be clamped
   * to the range of (0, height).
   */
  const void* computeOffset(const void* pixels, int x, int y) const;

  /**
   * Returns writable pixel address at (x, y) of specified base pixel address.
   * Note: The x value will be clamped to the range of [0, width-1], and the y value will be clamped
   * to the range of [0, height-1].
   */
  void* computeOffset(void* pixels, int x, int y) const;

 private:
  ImageInfo(int width, int height, ColorType colorType, AlphaType alphaType, size_t rowBytes)
      : _width(width),
        _height(height),
        _colorType(colorType),
        _alphaType(alphaType),
        _rowBytes(rowBytes) {
  }

  int _width = 0;
  int _height = 0;
  ColorType _colorType = ColorType::Unknown;
  AlphaType _alphaType = AlphaType::Unknown;
  size_t _rowBytes = 0;
};
}  // namespace tgfx