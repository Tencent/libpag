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

#include "gpu/TextureSampler.h"
#include "tgfx/gpu/Texture.h"

namespace tgfx {
class ProxyProvider;

/**
 * This class delays the acquisition of textures until they are actually required.
 */
class TextureProxy {
 public:
  virtual ~TextureProxy();

  /**
   * Returns the width of the texture proxy.
   */
  virtual int width() const;

  /**
   * Returns the height of the texture proxy.
   */
  virtual int height() const;

  /**
   * If we are instantiated and have a texture, return the mipmap state of that texture. Otherwise
   * returns the proxy's mipmap state from creation time.
   */
  virtual bool hasMipmaps() const;

  /**
   * Returns the the Texture of the proxy. Returns nullptr if the proxy is not instantiated yet.
   */
  std::shared_ptr<Texture> getTexture() const;

  /**
   * Returns true if the backing texture is instantiated.
   */
  bool isInstantiated() const;

  /**
   * Actually instantiate the backing texture, if necessary. Returns true if the backing texture is
   * instantiated.
   */
  bool instantiate();

  /**
   * Assigns a Cacheable owner to the proxy. The proxy will be findable via this owner using
   * ProxyProvider.findProxyByOwner(). If the updateTextureOwner is true, it will also assign the
   * owner to the internal texture.
   */
  void assignProxyOwner(const Cacheable* owner, bool updateTextureOwner = true);

  /*
   * Removes the Cacheable owner from the proxy. If the updateTexture is true, it will also remove
   * the owner from the target texture.
   */
  void removeProxyOwner(bool updateTexture = true);

 protected:
  ProxyProvider* provider = nullptr;
  std::shared_ptr<Texture> texture = nullptr;

  explicit TextureProxy(ProxyProvider* provider, std::shared_ptr<Texture> texture = nullptr);

  /**
   * Overrides to create a new Texture associated with specified context.
   */
  virtual std::shared_ptr<Texture> onMakeTexture(Context* context);

 private:
  uint32_t proxyOwnerID = 0;
  std::weak_ptr<Cacheable> cacheOwner;
  std::weak_ptr<TextureProxy> weakThis;

  friend class ProxyProvider;
};
}  // namespace tgfx
