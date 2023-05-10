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

#include "core/ImageStream.h"
#include "core/PixelBuffer.h"

namespace tgfx {
/**
 * This class is the smart container for pixel memory, and is used with Bitmap.
 */
class PixelRef : public ImageStream {
 public:
  /**
   * Creates a new PixelRef object width specified width and height. Returns nullptr if width or
   * height is not greater than zero.
   * @param width pixel column count, must be greater than zero.
   * @param height pixel row count, must be greater than zero.
   * @param alphaOnly If true, sets colorType to ColorType::ALPHA_8, otherwise sets to the native
   * 32-bit color type of the current platform.
   * @param tryHardware If true, a PixelRef backed by hardware is returned if it is available on
   * the current platform. Otherwise, a raster PixelRef is returned.
   */
  static std::shared_ptr<PixelRef> Make(int width, int height, bool alphaOnly = false,
                                        bool tryHardware = true);

  /**
   * Creates a new PixelRef with the specified PixelBuffer. Returns nullptr if the pixelBuffer is
   * nullptr.
   */
  static std::shared_ptr<PixelRef> Wrap(std::shared_ptr<PixelBuffer> pixelBuffer);

  /**
   * Returns an ImageInfo describing the width, height, color type, alpha type, and row bytes of the
   * PixelRef.
   */
  const ImageInfo& info() const {
    return pixelBuffer->info();
  }

  int width() const override {
    return pixelBuffer->width();
  }

  int height() const override {
    return pixelBuffer->height();
  }

  bool isAlphaOnly() const override {
    return pixelBuffer->isAlphaOnly();
  }

  bool isHardwareBacked() const override {
    return pixelBuffer->isHardwareBacked();
  }

  /**
   * Retrieves the backing hardware buffer. This method does not acquire any additional reference to
   * the returned hardware buffer. Returns nullptr if the PixelRef is not backed by a hardware
   * buffer.
   */
  HardwareBufferRef getHardwareBuffer() const {
    return pixelBuffer->getHardwareBuffer();
  }

  /**
   * Locks and returns the address of the read-only pixels to ensure that the memory is accessible.
   */
  const void* lockPixels() const {
    return pixelBuffer->lockPixels();
  }

  /**
   * Locks and returns the address of the writable pixels to ensure that the memory is accessible.
   */
  void* lockWritablePixels();

  /**
   * Call this to balance a successful call to lockPixels().
   */
  void unlockPixels() const {
    pixelBuffer->unlockPixels();
  }

  /**
   * Returns an ImageBuffer object capturing the pixels in the PixelRef. Subsequent writing of the
   * PixelRef will not be captured.
   */
  std::shared_ptr<ImageBuffer> makeBuffer() const {
    return pixelBuffer;
  }

  /**
   * Replaces all pixel values with transparent colors.
   */
  void clear();

 protected:
  std::shared_ptr<Texture> onMakeTexture(Context* context, bool mipMapped) override;

  bool onUpdateTexture(std::shared_ptr<Texture> texture, const Rect& bounds) override;

 private:
  std::shared_ptr<PixelBuffer> pixelBuffer = nullptr;

  explicit PixelRef(std::shared_ptr<PixelBuffer> pixelBuffer);
};
}  // namespace tgfx
