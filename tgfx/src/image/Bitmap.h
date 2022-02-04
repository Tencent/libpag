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

#include "EncodedFormat.h"
#include "ImageInfo.h"
#include "core/Data.h"
#include "pag/types.h"

namespace pag {
class Texture;

class Context;

class PixelBuffer;

/**
 * Bitmap describes a two-dimensional raster pixel array. Bitmap is not thread safe. Each thread
 * must have its own copy of Bitmap fields, although threads may share the underlying pixel array.
 */
class Bitmap {
 public:
  /**
   * Creates an empty Bitmap without pixels, with ColorType::Unknown, AlphaType::Unknown, and with
   * a width and height of zero.
   */
  Bitmap();

  /**
   * Creates an new Bitmap with specified PixelBuffer object.
   */
  explicit Bitmap(std::shared_ptr<PixelBuffer> pixelBuffer);

  /**
   * Allocates pixel memory for this bitmap with specified width and height.
   * @param width pixel column count, must be greater than zero.
   * @param height pixel row count, must be greater than zero.
   * @param alphaOnly If true, sets colorType to ColorType::ALPHA_8, otherwise sets to the native
   * 32-bit color type of current platform.
   * @param tryHardware If true, Bitmap will try to allocate a hardware PixelBuffer as the backing
   * buffer if it is available on current platform, otherwise a raster PixelBuffer is allocated.
   * @return Returns true if pixel memory is allocated successfully.
   */
  bool allocPixels(int width, int height, bool alphaOnly = false, bool tryHardware = true);

  /**
   * Resets to its initial state, as if Bitmap had been initialized by Bitmap(). Sets width, height,
   * row bytes to zero; pixel address to nullptr; color type to ColorType::Unknown; and alpha type
   * to AlphaType::Unknown. If PixelBuffer is allocated, its reference count is decreased by one,
   * releasing its memory if Bitmap is the sole owner.
   */
  void reset();

  /**
   * Returns true if this bitmap has not allocated pixel memory.
   */
  bool isEmpty() const;

  /**
   * Returns a ImageInfo describing the width, height, color type, alpha type, and row bytes of the
   * PixelMap.
   */
  const ImageInfo& info() const;

  /**
   * Returns the width of this bitmap.
   */
  int width() const;

  /**
   * Returns the height of this bitmap.
   */
  int height() const;

  /**
   * Returns the AlphaType of the Bitmap, which is always AlphaType::Premultiplied.
   */
  AlphaType alphaType() const;

  /**
   * Returns the ColorType of the Bitmap.
   */
  ColorType colorType() const;

  /**
   * Returns the rowBytes of the Bitmap.
   */
  size_t rowBytes() const;

  /**
   * Returns the byte size of the Bitmap.
   */
  size_t byteSize() const;

  /**
   * Returns true if this bitmap has a HardwareBuffer as pixel storage.
   */
  bool isHardwareBacked() const {
    return hardwareBacked;
  }

  /**
   * Locks and returns the address of the pixels to ensure that the memory is accessible.
   */
  void* lockPixels() const;

  /**
   * Call this to balance a successful call to lockPixels().
   */
  void unlockPixels() const;

  /**
   * Replaces all pixel values with transparent colors.
   */
  void eraseAll();

  /**
   * Encodes the pixels in Bitmap into a binary image format.
   * @param format One of: EncodedFormat::JPEG, EncodedFormat::PNG, EncodedFormat::WEBP
   * @param quality A platform and format specific metric trading off size and encoding error. When
   * used, quality equaling 100 encodes with the least error. quality may be ignored by the encoder.
   * @return Returns nullptr if encoding fails, or if format is not supported.
   */
  std::shared_ptr<Data> encode(EncodedFormat format = EncodedFormat::PNG, int quality = 100) const;

  /**
   * Creates a new Texture capturing bitmap contents for the context that is current on the calling
   * thread.
   */
  std::shared_ptr<Texture> makeTexture(Context* context) const;

  /**
   * Copies a rect of pixels to dstPixels with specified ImageInfo. Copy starts at (srcX, srcY), and
   * does not exceed Bitmap (width(), height()). Pixels are copied only if pixel conversion is
   * possible. Returns true if pixels are copied to dstPixels.
   */
  bool readPixels(const ImageInfo& dstInfo, void* dstPixels, int srcX = 0, int srcY = 0) const;

  /**
   * Copies a rect of pixels from src. Copy starts at (dstX, dstY), and does not exceed
   * Bitmap (width(), height()). Pixels are copied only if pixel conversion is possible.
   * Returns true if src pixels are copied to Bitmap.
   */
  bool writePixels(const ImageInfo& srcInfo, const void* srcPixels, int dstX = 0, int dstY = 0);

 private:
  bool hardwareBacked = false;
  std::shared_ptr<PixelBuffer> pixelBuffer = nullptr;
};

/**
 * BitmapLock provides a convenient RAII-style mechanism for locking pixels of bitmap for the
 * duration of a scoped block.
 */
class BitmapLock {
 public:
  explicit BitmapLock(const Bitmap& bitmap) : bitmap(bitmap) {
    _pixels = bitmap.lockPixels();
  }

  ~BitmapLock() {
    bitmap.unlockPixels();
  }

  /**
   * Returns the locked address of bitmap pixels.
   */
  void* pixels() const {
    return _pixels;
  }

 private:
  void* _pixels = nullptr;
  Bitmap bitmap = {};
};

/**
 * Trace() provides a utility to view the Bitmap.
 */
void Trace(const Bitmap& bitmap, const std::string& tag = "");
}  // namespace pag