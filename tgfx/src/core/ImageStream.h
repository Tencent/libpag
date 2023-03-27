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

#include <memory>
#include <mutex>
#include <vector>
#include "tgfx/core/Rect.h"

namespace tgfx {
class ImageReader;
class Texture;
class Context;

/**
 * ImageStream represents a writable pixel buffer that can continuously generate ImageBuffer
 * objects, which can be directly accessed by The ImageReader class. ImageStream is an abstract
 * class. Use its subclasses instead.
 */
class ImageStream {
 public:
  virtual ~ImageStream() = default;

  /**
   * Returns the width of the ImageStream.
   */
  virtual int width() const = 0;

  /**
   * Returns the height of the ImageStream.
   */
  virtual int height() const = 0;

  /**
   * Returns true if pixels represent transparency only. If true, each pixel is packed in 8 bits as
   * defined by ColorType::ALPHA_8.
   */
  virtual bool isAlphaOnly() const = 0;

  /**
   * Returns true if the ImageStream is backed by a platform-specified hardware buffer. Hardware
   * buffers allow sharing memory across CPU and GPU, which can be used to speed up the texture
   * uploading.
   */
  virtual bool isHardwareBacked() const = 0;

  /**
   * Marks the specified bounds as dirty. The next time the texture is updated, the pixels in
   * the specified bounds will be uploaded to the texture.
   */
  void markContentDirty(const Rect& bounds);

 protected:
  /**
   * Creates a new Texture capturing the pixels in the TextureBuffer. The mipMapped parameter
   * specifies whether created texture must allocate mip map levels.
   */
  virtual std::shared_ptr<Texture> onMakeTexture(Context* context, bool mipMapped) = 0;

  /**
   * Updates the specified bounds of the texture with the pixels in the TextureBuffer.
   */
  virtual bool onUpdateTexture(std::shared_ptr<Texture> texture, const Rect& bounds) = 0;

 private:
  std::mutex locker = {};
  std::vector<ImageReader*> readers = {};

  void attachToStream(ImageReader* imageReader);

  void detachFromStream(ImageReader* imageReader);

  friend class ImageReader;
};
}  // namespace tgfx
