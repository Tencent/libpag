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
#include "tgfx/core/ImageBuffer.h"
#include "tgfx/core/ImageGenerator.h"
#include "tgfx/gpu/Context.h"
#include "tgfx/gpu/SurfaceOptions.h"

namespace tgfx {
class UniqueKey;

/**
 * ImageSource generates texture proxies for images.
 */
class ImageSource {
 public:
  /**
   * Creates ImageSource from an image generator. ImageSource is returned if the generator is not
   * nullptr. The image generator may wrap codec data or custom data.
   */
  static std::shared_ptr<ImageSource> MakeFrom(UniqueKey uniqueKey,
                                               std::shared_ptr<ImageGenerator> generator);

  /**
   * Creates ImageSource from ImageBuffer, ImageSource is returned if the imageBuffer is not nullptr
   * and its dimensions are greater than zero.
   */
  static std::shared_ptr<ImageSource> MakeFrom(UniqueKey uniqueKey,
                                               std::shared_ptr<ImageBuffer> buffer);

  /**
   * Creates ImageSource from Texture, ImageSource is returned if texture is not nullptr. Note that
   * this method is not thread safe, must be called while the asscociated context is locked.
   */
  static std::shared_ptr<ImageSource> MakeFrom(UniqueKey uniqueKey,
                                               std::shared_ptr<Texture> texture);

  virtual ~ImageSource() = default;

  /**
   * Returns the width of the target image.
   */
  virtual int width() const = 0;

  /**
   * Returns the height of the target image.
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
   * Returns true if ImageSource is backed by an image generator or other services that create its
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
   * Retrieves the backend texture. Returns an invalid BackendTexture if the ImageSource is not
   * backed by a Texture.
   */
  virtual BackendTexture getBackendTexture() const {
    return {};
  }

  /**
   * Returns an ImageSource backed by GPU texture associated with context. Returns original
   * ImageSource if context is compatible with backing GPU texture. Returns nullptr if context is
   * nullptr, or if ImageSource was created with another context.
   */
  virtual std::shared_ptr<ImageSource> makeTextureSource(Context* context) const;

  /**
   * Returns a decoded ImageSource from the lazy ImageSource. The returned ImageSource shares the
   * same texture cache with the original ImageSource and immediately schedules an asynchronous
   * decoding task, which will not block the calling thread. Returns nullptr if the ImageSource is
   * not lazy or has a corresponding texture cache in the specified context.
   */
  std::shared_ptr<ImageSource> makeDecoded(Context* context = nullptr) const;

  /**
   * Returns an Image with mipmaps enabled. Returns the original Image if the Image has mipmaps
   * enabled already or fails to enable mipmaps.
   */
  std::shared_ptr<ImageSource> makeMipMapped() const;

  /**
   * Returns a TextureProxy if there is a corresponding cache in the context. Otherwise, immediately
   * creates one.
   */
  std::shared_ptr<TextureProxy> lockTextureProxy(Context* context, uint32_t surfaceFlags = 0) const;

 protected:
  UniqueKey uniqueKey = {};
  std::weak_ptr<ImageSource> weakThis;

  explicit ImageSource(UniqueKey uniqueKey);

  virtual std::shared_ptr<ImageSource> onMakeDecoded(Context* context) const;

  virtual std::shared_ptr<ImageSource> onMakeMipMapped() const = 0;

  virtual std::shared_ptr<TextureProxy> onMakeTextureProxy(Context* context,
                                                           uint32_t surfaceFlags) const = 0;
};
}  // namespace tgfx
