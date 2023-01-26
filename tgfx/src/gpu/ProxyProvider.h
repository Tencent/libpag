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

#include "gpu/proxies/TextureProxy.h"
#include "tgfx/core/ImageBuffer.h"

namespace tgfx {
class CacheOwnerTextureProxy;

/**
 * A factory for creating TextureProxy-derived objects.
 */
class ProxyProvider {
 public:
  explicit ProxyProvider(Context* context);

  /*
   * Finds a proxy by cache owner.
   */
  std::shared_ptr<TextureProxy> findProxyByOwner(const Cacheable* owner);

  /*
   * Create a texture proxy for the image buffer. The image buffer will be released after being
   * uploaded to the GPU.
   */
  std::shared_ptr<TextureProxy> createTextureProxy(std::shared_ptr<ImageBuffer> imageBuffer,
                                                   bool mipMapped = false);

  /**
   * Create a TextureProxy without any data.
   */
  std::shared_ptr<TextureProxy> createTextureProxy(int width, int height, PixelFormat format,
                                                   ImageOrigin origin = ImageOrigin::TopLeft,
                                                   bool mipMapped = false);

  /*
   * Create a texture proxy that wraps an existing texture.
   */
  std::shared_ptr<TextureProxy> wrapTexture(std::shared_ptr<Texture> texture);

 private:
  Context* context = nullptr;
  std::unordered_map<uint32_t, CacheOwnerTextureProxy*> cacheOwnerMap = {};

  void changeCacheOwner(CacheOwnerTextureProxy* proxy, const Cacheable* owner);

  void removeCacheOwner(CacheOwnerTextureProxy* proxy);

  friend class CacheOwnerTextureProxy;
};
}  // namespace tgfx
