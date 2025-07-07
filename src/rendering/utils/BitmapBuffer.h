/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "pag/pag.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/platform/HardwareBuffer.h"
#include "tgfx/platform/NativeImage.h"

namespace pag {
/**
 * BitmapBuffer is a wrapper of raster pixels or a hardware buffer. BitmapBuffer does not try to
 * manage the lifetime of the pixel memory.
 */
class BitmapBuffer {
 public:
  /**
   * Creates a new BitmapBuffer from the specified hardware buffer.
   */
  static std::shared_ptr<BitmapBuffer> Wrap(HardwareBufferRef hardwareBuffer);

  /**
   * Creates a new BitmapBuffer from the specified image info and pixels.
   */
  static std::shared_ptr<BitmapBuffer> Wrap(const tgfx::ImageInfo& info, void* pixels);

  /**
   * Returns the width of the bitmap buffer.
   */
  int width() const {
    return _info.width();
  }

  /**
   * Returns the height of the bitmap buffer.
   */
  int height() const {
    return _info.height();
  }

  /**
   * Returns the image info of the bitmap buffer.
   */
  const tgfx::ImageInfo& info() const {
    return _info;
  }

  /**
   * Retrieves the backing hardware buffer. This method does not acquire any additional reference to
   * the returned hardware buffer. Returns nullptr if the BitmapBuffer is not backed by a hardware
   * buffer.
   */
  HardwareBufferRef getHardwareBuffer() const;

  /**
   * Locks and returns the writable pixels, the base address corresponding to the bitmap origin.
   */
  void* lockPixels();

  /**
   * Call this to balance a successful call to lockPixels().
   */
  void unlockPixels();

 private:
  tgfx::ImageInfo _info = {};
  bool hardwareBacked = false;
  void* pixels = nullptr;
  HardwareBufferRef hardwareBuffer = nullptr;

  explicit BitmapBuffer(const tgfx::ImageInfo& info);
};
}  // namespace pag
