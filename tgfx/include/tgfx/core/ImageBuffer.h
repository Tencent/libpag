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
 * ImageBuffer describes a two dimensional array of pixels which is optimized for creating textures.
 * ImageBuffer is immutable and safe across threads.
 */
class ImageBuffer {
 public:
  virtual ~ImageBuffer() = default;
  /**
   * Returns the width of the image buffer.
   */
  virtual int width() const = 0;

  /**
   * Returns the height of the image buffer.
   */
  virtual int height() const = 0;

  /**
   * Returns true if pixels represent transparency only. If true, each pixel is packed in 8 bits as
   * defined by ColorType::ALPHA_8.
   */
  virtual bool isAlphaOnly() const = 0;

  /**
   * Creates a new Texture capturing the pixels in this image buffer. The optional mipMapped
   * parameter specifies whether created texture must allocate mip map levels.
   */
  std::shared_ptr<Texture> makeTexture(Context* context, bool mipMapped = false) const {
    return onMakeTexture(context, mipMapped);
  }

 protected:
  ImageBuffer() = default;

  virtual std::shared_ptr<Texture> onMakeTexture(Context* context, bool mipMapped) const = 0;
};
}  // namespace tgfx
