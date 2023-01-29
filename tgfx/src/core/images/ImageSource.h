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

#include "gpu/ProxyProvider.h"
#include "gpu/proxies/TextureProxy.h"
#include "tgfx/core/Cacheable.h"
#include "tgfx/core/ImageBuffer.h"
#include "tgfx/core/ImageGenerator.h"
#include "tgfx/gpu/Context.h"

namespace tgfx {
/**
 * ImageSource generates texture proxies for images.
 */
class ImageSource : public Cacheable {
 public:
  /**
   * Creates ImageSource from an image generator. ImageSource is returned if generator is not
   * nullptr. Image generator may wrap codec data or custom data.
   */
  static std::shared_ptr<ImageSource> MakeFromGenerator(std::shared_ptr<ImageGenerator> generator,
                                                        bool mipMapped);

  /**
   * Creates ImageSource from ImageBuffer, ImageSource is returned if imageBuffer is not nullptr and
   * its dimensions are greater than zero.
   */
  static std::shared_ptr<ImageSource> MakeFromBuffer(std::shared_ptr<ImageBuffer> buffer,
                                                     bool mipMapped);

  /**
   * Creates ImageSource from Texture, ImageSource is returned if texture is not nullptr.
   */
  static std::shared_ptr<ImageSource> MakeFromTexture(std::shared_ptr<Texture> texture);

  /**
   * Returns the width of target image.
   */
  virtual int width() const = 0;

  /**
   * Returns the height of target image.
   */
  virtual int height() const = 0;

  /**
   * Returns true if the ImageSource has mipmap levels. The value was passed in when creating the
   * ImageSource. It may be ignored if mipmaps are not supported by the GPU or the ImageSource.
   */
  virtual bool hasMipmaps() const = 0;

  /**
   * Returns true if pixels represent transparency only. If true, each pixel is packed in 8 bits as
   * defined by ColorType::ALPHA_8.
   */
  virtual bool isAlphaOnly() const = 0;

  /**
   * Returns true if ImageSource is backed by an image generator or other service that creates its
   * pixels on-demand.
   */
  virtual bool isLazyGenerated() const {
    return false;
  }

  /**
   * Returns true if the ImageSource was created from a GPU texture.
   */
  virtual bool isTextureBacked() const {
    return false;
  }

  /**
   * Retrieves the backend texture. Returns nullptr if ImageSource is not texture backed.
   */
  virtual std::shared_ptr<Texture> getTexture() const {
    return nullptr;
  }

  /**
   * Returns a decoded ImageSource from the lazy ImageSource. The returned ImageSource shares the
   * same texture cache with the original ImageSource and immediately schedules an asynchronous
   * decoding task, which will not block the calling thread. Returns nullptr if the ImageSource is
   * not lazy or has a corresponding texture cache in the specified context.
   */
  std::shared_ptr<ImageSource> makeDecodedSource(Context* context = nullptr) const;

  /**
   * Returns an ImageSource backed by GPU texture associated with context. Returns original
   * ImageSource if context is compatible with backing GPU texture. Returns nullptr if context is
   * nullptr, or if ImageSource was created with another context.
   */
  std::shared_ptr<ImageSource> makeTextureSource(Context* context, bool wrapCacheOnly) const;

  /**
   * Creates a TextureProxy with the specified context from the ImageSource.
   */
  virtual std::shared_ptr<TextureProxy> lockTextureProxy(Context* context) const;

 protected:
  virtual std::shared_ptr<ImageSource> onMakeDecodedSource(Context* context) const;

  virtual std::shared_ptr<TextureProxy> onMakeTextureProxy(Context* context) const = 0;
};
}  // namespace tgfx
