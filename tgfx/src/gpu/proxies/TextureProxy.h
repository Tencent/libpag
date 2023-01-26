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
 * This class delays the acquisition of textures until they are actually required.
 */
class TextureProxy {
 public:
  /*
   * Create a TextureProxy that wraps an existing texture. Different from the texture proxies
   * created from a ProxyProvider, the returned TextureProxy is thread safe and does not implement
   * the cache-owner methods.
   */
  static std::shared_ptr<TextureProxy> Wrap(std::shared_ptr<Texture> texture);

  virtual ~TextureProxy() = default;

  /**
   * Returns the width of the texture proxy.
   */
  virtual int width() const {
    return texture->width();
  }

  /**
   * Returns the height of the texture proxy.
   */
  virtual int height() const {
    return texture->height();
  }

  /**
   * If we are instantiated and have a texture, return the mip state of that texture. Otherwise
   * returns the proxy's mip state from creation time.
   */
  virtual bool hasMipmaps() const {
    return texture->getSampler()->mipMapped();
  }

  /**
   * Creates a new Texture associated with specified context. Returns nullptr if failed.
   */
  std::shared_ptr<Texture> getTexture() const {
    return texture;
  }

  /**
   * Returns true if the backing texture is instantiated.
   */
  bool isInstantiated() const {
    return texture != nullptr;
  }

  /**
   * Actually instantiate the backing texture, if necessary. Returns true if the backing texture is
   * instantiated.
   */
  virtual bool instantiate();

  /**
   * Assigns a cache owner to the proxy. The proxy will be findable via this owner using
   * ProxyProvider.findProxyByOwner(). If the proxy has already been instantiated, it will also
   * assign the owner to the target texture. Does nothing if this proxy was not created by a
   * ProxyProvider.
   */
  virtual void assignCacheOwner(const Cacheable* owner);

  /*
   * Removes the cache owner from a proxy. If the proxy has already been instantiated, it will
   * also remove the owner from the target texture. Does nothing if this proxy was not created by a
   * ProxyProvider.
   */
  virtual void removeCacheOwner();

 protected:
  std::shared_ptr<Texture> texture = nullptr;

  explicit TextureProxy(std::shared_ptr<Texture> texture);
};
}  // namespace tgfx
