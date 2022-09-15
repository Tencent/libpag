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

#include "tgfx/gpu/Texture.h"

namespace tgfx {
/**
 * A ImageBuffer contains an array of fully processed pixels, suitable for immediate texture
 * uploading on the GPU.
 */
class ImageBuffer {
 public:
  virtual ~ImageBuffer() = default;
  /**
   * Returns the width of the image buffer.
   */
  int width() const {
    return _width;
  }

  /**
   * Returns the height of the image buffer.
   */
  int height() const {
    return _height;
  }

  /**
   * Creates a new Texture capturing the pixels in this image buffer.
   */
  virtual std::shared_ptr<Texture> makeTexture(Context* context) const = 0;

  /**
   * Returns whether the current buffer can make mipMap textures.
   */
  virtual bool mipMapSupport() const {
    return false;
  }

  /**
   * Creates a new Texture with mip map levels capturing the pixels in this image buffer.
   * If mip maps are not supported by the GPU, the texture will be created without mip maps.
   */
  virtual std::shared_ptr<Texture> makeMipMappedTexture(Context*) const {
    return nullptr;
  }

 protected:
  ImageBuffer(int width, int height) : _width(width), _height(height) {
  }

 private:
  int _width = 0;
  int _height = 0;
};
}  // namespace tgfx
