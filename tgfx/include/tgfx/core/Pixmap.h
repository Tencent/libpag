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

#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Rect.h"

namespace tgfx {
/**
 * Pixmap provides a utility to pair ImageInfo width pixels. Pixmap is a low-level class that
 * provides convenience functions to access raster destinations, which can convert the format of
 * pixels from one to another. Pixmap does not try to manage the lifetime of the pixel memory. Use
 * Bitmap to manage pixel memory. Pixmap is not thread safe.
 */
class Pixmap {
 public:
  /**
   * Creates an empty Pixmap without pixels, and with an empty ImageInfo.
   */
  Pixmap() = default;

  /**
   * Creates a new Pixmap with the specified ImageInfo and read-only pixels. Not that the caller
   * must ensure that the pixel memory is valid for the lifetime of the Pixmap.
   */
  Pixmap(const ImageInfo& info, const void* pixels);

  /**
   * Creates a new Bitmap with specified ImageInfo and writable pixels. Not that the caller must
   * ensure that the pixel memory is valid for the lifetime of the Pixmap.
   */
  Pixmap(const ImageInfo& info, void* pixels);

  /**
   * Creates a new Pixmap from the specified read-only Bitmap. Pixmap will lock pixels from the
   * Bitmap and hold a reference to it. The Bitmap will remain locked until the Pixmap goes out of
   * scope or reset. Not that the caller must ensure that the Bitmap is valid for the lifetime of
   * the Pixmap.
   */
  explicit Pixmap(const Bitmap& bitmap);

  /**
   * Creates a new Pixmap from the specified writable Bitmap. Pixmap will lock pixels from the
   * Bitmap and hold a reference to it. The Bitmap will remain locked until the Pixmap goes out of
   * scope or reset. Note that the caller must ensure that the Bitmap is valid for the lifetime of
   * the Pixmap.
   */
  explicit Pixmap(Bitmap& bitmap);

  //TODO(domrjchen): remove this method.
  explicit Pixmap(std::shared_ptr<PixelBuffer> pixelBuffer);

  ~Pixmap();

  /**
   * Sets the ImageInfo to empty, and pixels to nullptr.
   */
  void reset();

  /**
   * Set the Pixmap to the specified info and pixels. The memory lifetime of pixels is managed by
   * the caller. When Pixmap goes out of scope, the memory of pixels is unaffected.
   */
  void reset(const ImageInfo& info, const void* pixels);

  /**
   * Set the Pixmap to the specified ImageInfo and writable pixels. The memory lifetime of pixels is
   * managed by the caller. When Pixmap goes out of scope, the memory of pixels is unaffected.
   */
  void reset(const ImageInfo& info, void* pixels);

  /**
   * Return true if the Pixmap describes an empty area of pixels.
   */
  bool isEmpty() const {
    return _info.isEmpty();
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
   * Returns the pixel address, the base address corresponding to the pixel origin.
   */
  const void* pixels() const {
    return _pixels;
  }

  /**
   * Returns the writable pixel address, the base address corresponding to the pixel origin. Returns
   * nullptr if the Pixmap is constructed from read-only pixels.
   */
  void* writablePixels() const {
    return _writablePixels;
  }

  /**
   * Returns pixel at (x, y) as unpremultiplied color. Some color precision may be lost in the
   * conversion to unpremultiplied color; original pixel data may have additional precision. Returns
   * a transparent color if the point (x, y) is not contained by bounds.
   * @param x  column index, zero or greater, and less than width()
   * @param y  row index, zero or greater, and less than height()
   * @return   pixel converted to unpremultiplied color
   */
  Color getColor(int x, int y) const;

  /**
   * Returns subset of the Pixmap. subset must be fully contained by Pixmap dimensions. Returns
   * an empty Pixmap if subset is empty, or subset is not contained by bounds.
   */
  Pixmap makeSubset(const Rect& subset) const;

  /**
   * Copies a rect of pixels to dstPixels with specified ImageInfo. Copy starts at (srcX, srcY), and
   * does not exceed Pixmap (width(), height()). Pixels are copied only if pixel conversion is
   * possible. Returns true if pixels are copied to dstPixels.
   */
  bool readPixels(const ImageInfo& dstInfo, void* dstPixels, int srcX = 0, int srcY = 0) const;

  /**
   * Copies a rect of pixels from src. Copy starts at (dstX, dstY), and does not exceed
   * Pixmap (width(), height()). Pixels are copied only if pixel conversion is possible and the
   * Pixmap is constructed from writable pixels. Returns true if src pixels are copied to Pixmap.
   */
  bool writePixels(const ImageInfo& srcInfo, const void* srcPixels, int dstX = 0, int dstY = 0);

  /**
   * Replaces all pixel values with transparent colors. Returns false if the Pixmap is constructed
   * from read-only pixels.
   */
  bool eraseAll();

 private:
  ImageInfo _info = {};
  const void* _pixels = nullptr;
  void* _writablePixels = nullptr;
  PixelBuffer* pixelBuffer = nullptr;

  void reset(PixelBuffer* pixelBuffer);
};
}  // namespace tgfx