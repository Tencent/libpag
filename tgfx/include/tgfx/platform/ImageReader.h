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

#include <atomic>
#include "tgfx/core/ImageBuffer.h"

namespace tgfx {
class Texture;

/**
 * The ImageReader class allows direct access to image buffers generated from a video-related object
 * of the native platform. The video-related object could be a Surface on the android platform or an
 * HTMLVideoElement on the web platform. ImageReader is an abstract class. Use its subclass on the
 * native platform instead.
 */
class ImageReader {
 public:
  virtual ~ImageReader() = default;

  /**
   * Returns the width of generated image buffers.
   */
  int width() const {
    return _width;
  }

  /**
   * Returns the height of generated image buffers.
   */
  int height() const {
    return _height;
  }

 protected:
  std::weak_ptr<ImageReader> weakThis;
  std::shared_ptr<Texture> texture = nullptr;

  ImageReader(int width, int height) : _width(width), _height(height) {
  }

  /**
   * Creates a new ImageBuffer from the reader and immediately marks the previously returned
   * ImageBuffer as invalid.
   */
  std::shared_ptr<ImageBuffer> makeNextBuffer();

  virtual bool onUpdateTexture(Context* context, bool mipMapped) = 0;

 private:
  int _width = 0;
  int _height = 0;
  std::atomic_int textureVersion = 0;

  std::shared_ptr<Texture> readTexture(int bufferVersion, Context* context, bool mipMapped);

  friend class ImageReaderBuffer;
};
}  // namespace tgfx
