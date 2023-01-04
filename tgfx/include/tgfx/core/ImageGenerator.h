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

#include "tgfx/core/ImageBuffer.h"

namespace tgfx {
/**
 * ImageGenerator is the base class for decoding images from codec data or custom data.
 */
class ImageGenerator {
 public:
  virtual ~ImageGenerator() = default;

  /**
   * Returns the width of target image.
   */
  int width() const {
    return _width;
  }

  /**
   * Returns the height of target image.
   */
  int height() const {
    return _height;
  }

  /**
   * Crates a new image buffer capturing the pixels decoded from this image generator.
   * ImageGenerator do not cache the returned image buffer, each call to this method allocates
   * additional storage.
   */
  virtual std::shared_ptr<ImageBuffer> makeBuffer() const = 0;

 protected:
  ImageGenerator(int width, int height) : _width(width), _height(height) {
  }

 private:
  int _width = 0;
  int _height = 0;
};
}  // namespace tgfx
