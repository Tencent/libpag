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
#include "tgfx/core/EncodedFormat.h"
#include "tgfx/core/ImageInfo.h"
#include "tgfx/core/PixelBuffer.h"

namespace tgfx {
/**
 * Bitmap is a low level class which provides convenience functions to access raster destinations,
 * which provides a utility to convert the format of pixels from one to another or compress the
 * pixels into a image file.
 */
class Bitmap {
 public:
  /**
   * Creates a new Bitmap with specified ImageInfo and read-only pixels. The returned bitmap does
   * not try to manage the lifetime of the pixel memory.
   * Note: All non-const methods are invalid if the bitmap constructed from read-only pixels, such
   * as writePixels() and eraseAll().
   */
  Bitmap(const ImageInfo& info, const void* pixels);

  /**
   * Creates a new Bitmap with specified ImageInfo and writable pixels. The returned bitmap does not
   * try to manage the lifetime of the pixel memory.
   */
  Bitmap(const ImageInfo& info, void* pixels);

  /**
   * Creates an new Bitmap with specified PixelBuffer object. Bitmap will hold a reference to The
   * PixelBuffer and lock pixels from it. The PixelBuffer will remain locked until the returned
   * bitmap is destructed or reset.
   */
  explicit Bitmap(std::shared_ptr<PixelBuffer> pixelBuffer);

  ~Bitmap();

  /**
   * Resets the bitmap to empty. If the pixels in this bitmap are locked from a PixelBuffer, the
   * unlockPixels() of the PixelBuffer will be called immediately.
   */
  void reset();

  /**
   * Return true if this Bitmap describes an empty area of pixels.
   */
  bool isEmpty() const;

  /**
   * Returns a ImageInfo describing the width, height, color type, alpha type, and row bytes of the
   * pixels.
   */
  const ImageInfo& info() const;

  /**
   * Returns the width of the pixels.
   */
  int width() const;

  /**
   * Returns the height of the pixels.
   */
  int height() const;

  /**
   * Returns the ColorType of the pixels.
   */
  ColorType colorType() const;

  /**
   * Returns the AlphaType of the pixels.
   */
  AlphaType alphaType() const;

  /**
   * Returns the rowBytes of the pixels.
   */
  size_t rowBytes() const;

  /**
   * Returns the byte size of the pixels.
   */
  size_t byteSize() const;

  /**
   * Returns the read-only pixel address, the base address corresponding to the pixel origin.
   */
  const void* pixels() const;

  /**
   * Returns the writable pixel address, the base address corresponding to the pixel origin. Returns
   * nullptr if the bitmap is constructed from read-only pixels.
   */
  void* writablePixels() const;

  /**
   * Encodes the pixels in Bitmap into a binary image format.
   * @param format One of: EncodedFormat::JPEG, EncodedFormat::PNG, EncodedFormat::WEBP
   * @param quality A platform and format specific metric trading off size and encoding error. When
   * used, quality equaling 100 encodes with the least error. quality may be ignored by the encoder.
   * @return Returns nullptr if encoding fails, or if format is not supported.
   */
  std::shared_ptr<Data> encode(EncodedFormat format = EncodedFormat::PNG, int quality = 100) const;

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
   * Replaces all pixel values with transparent colors. Returns false if the bitmap is constructed
   * from read-only pixels.
   */
  bool eraseAll();

 private:
  ImageInfo _info = {};
  const void* _pixels = nullptr;
  void* _writablePixels = nullptr;
  std::shared_ptr<PixelBuffer> pixelBuffer = nullptr;
};
}  // namespace tgfx