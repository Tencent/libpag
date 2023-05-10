/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "tgfx/core/Color.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/EncodedFormat.h"
#include "tgfx/core/ImageBuffer.h"
#include "tgfx/core/ImageInfo.h"
#include "tgfx/platform/HardwareBuffer.h"

namespace tgfx {
class PixelRef;

/**
 * Bitmap describes a two-dimensional raster pixel array. Bitmap points to PixelRef, which describes
 * the physical array of pixels and is optimized for creating textures. If the pixel array is
 * primarily read-only, use Image for better performance. Declaring Bitmap const prevents altering
 * ImageInfo: the Bitmap height, width, and so on cannot change. It does not affect PixelRef: a
 * caller may write its pixels. Bitmap is not thread safe.
 */
class Bitmap {
 public:
  /**
   * Creates an empty Bitmap without pixels, and with an empty ImageInfo.
   */
  Bitmap() = default;

  /**
   * Creates a new Bitmap and tries to allocate its pixels by the specified width, height, and
   * native color type. If the alphaOnly is true, sets ImageInfo to ColorType::ALPHA_8. If the
   * tryHardware is true and there is hardware buffer support on the current platform, a hardware
   * backed PixelRef is allocated. Otherwise, a raster PixelRef is allocated. The isEmpty() method
   * of the Bitmap will return true if allocation fails.
   */
  Bitmap(int width, int height, bool alphaOnly = false, bool tryHardware = true);

  /**
   * Copies settings from src to returned Bitmap. Shares pixels if src has pixels allocated, so both
   * bitmaps reference the same pixels.
   */
  Bitmap(const Bitmap& src);

  /**
   * Copies settings from src to returned Bitmap. Moves ownership of src pixels to Bitmap.
   */
  Bitmap(Bitmap&& src);

  /**
   * Creates a new Bitmap from the platform-specific hardware buffer. For example, the hardware
   * buffer could be an AHardwareBuffer on the android platform or a CVPixelBufferRef on the apple
   * platform. The Bitmap takes a reference to the hardwareBuffer.
   */
  explicit Bitmap(HardwareBufferRef hardwareBuffer);

  /**
   * Copies settings from src to returned Bitmap. Shares pixels if src has pixels allocated, so both
   * bitmaps reference the same pixels.
   */
  Bitmap& operator=(const Bitmap& src);

  /**
   * Copies settings from src to returned Bitmap. Moves ownership of src pixels to Bitmap.
   */
  Bitmap& operator=(Bitmap&& src);

  /**
   * Sets ImageInfo to width, height, and the native color type; and allocates pixel memory. If
   * the alphaOnly is true, sets ImageInfo to ColorType::ALPHA_8. If the tryHardware is true and
   * there is hardware buffer support on the current platform, a hardware-backed PixelRef is
   * allocated. Otherwise, a raster PixelRef is allocated. Returns true if the PixelRef is
   * allocated successfully.
   */
  bool allocPixels(int width, int height, bool alphaOnly = false, bool tryHardware = true);

  /**
   * Locks and returns the writable pixels, the base address corresponding to the pixel origin.
   */
  void* lockPixels();

  /**
   * Locks and returns the read-only pixels, the base address corresponding to the pixel origin.
   */
  const void* lockPixels() const;

  /**
   * Call this to balance a successful call to lockPixels().
   */
  void unlockPixels() const;

  /**
   * Return true if the Bitmap describes an empty area of pixels.
   */
  bool isEmpty() const {
    return _info.isEmpty();
  }

  /**
   * Returns an ImageInfo describing the width, height, color type, alpha type, and row bytes of the
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
   * Returns true if pixels represent transparency only. If true, each pixel is packed in 8 bits as
   * defined by ColorType::ALPHA_8.
   */
  bool isAlphaOnly() const {
    return _info.isAlphaOnly();
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
   * Returns true if the Bitmap is backed by a platform-specified hardware buffer. A hardware-backed
   * Bitmap allows sharing buffers across CPU and GPU, which can be used to speed up the texture
   * uploading.
   */
  bool isHardwareBacked() const;

  /**
   * Retrieves the backing hardware buffer. This method does not acquire any additional reference to
   * the returned hardware buffer. Returns nullptr if the Bitmap is not backed by a hardware buffer.
   */
  HardwareBufferRef getHardwareBuffer() const;

  /**
   * Encodes the pixels in Bitmap into a binary image format.
   * @param format One of: EncodedFormat::JPEG, EncodedFormat::PNG, EncodedFormat::WEBP
   * @param quality A platform and format specific metric trading off size and encoding error. When
   * used, quality equaling 100 encodes with the least error. quality may be ignored by the encoder.
   * @return Returns nullptr if encoding fails, or if the format is not supported.
   */
  std::shared_ptr<Data> encode(EncodedFormat format = EncodedFormat::PNG, int quality = 100) const;

  /**
   * Returns pixel at (x, y) as unpremultiplied color. Some color precision may be lost in the
   * conversion to unpremultiplied color; original pixel data may have additional precision. Returns
   * a transparent color if the point (x, y) is not contained by bounds.
   */
  Color getColor(int x, int y) const;

  /**
   * Copies a rect of pixels to dstPixels with specified ImageInfo. Copy starts at (srcX, srcY), and
   * does not exceed Bitmap (width(), height()). Pixels are copied only if pixel conversion is
   * possible. Returns true if pixels are copied to dstPixels.
   */
  bool readPixels(const ImageInfo& dstInfo, void* dstPixels, int srcX = 0, int srcY = 0) const;

  /**
   * Copies a rect of pixels from src. Copy starts at (dstX, dstY), and does not exceed
   * Bitmap (width(), height()). Pixels are copied only if pixel conversion is possible and the
   * bitmap is constructed from writable pixels. Returns true if src pixels are copied to Bitmap.
   */
  bool writePixels(const ImageInfo& srcInfo, const void* srcPixels, int dstX = 0, int dstY = 0);

  /**
   * Replaces all pixel values with transparent colors.
   */
  void clear();

  /**
   * Returns an ImageBuffer object capturing the pixels in the Bitmap. Subsequent writing of the
   * Bitmap will not be captured. Instead, the Bitmap will copy its pixels to a new memory buffer if
   * there is a subsequent writing call to the Bitmap while the returned ImageBuffer is still alive.
   * If the Bitmap is modified frequently, create an ImageReader from the Bitmap instead, which
   * allows you to continuously read the latest content from the Bitmap with minimal memory copying.
   * Returns nullptr if the Bitmap is empty.
   */
  std::shared_ptr<ImageBuffer> makeBuffer() const;

 private:
  ImageInfo _info = {};
  std::shared_ptr<PixelRef> pixelRef = nullptr;

  friend class Pixmap;
  friend class ImageReader;
};
}  // namespace tgfx
