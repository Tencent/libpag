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

#include "gpu/Texture.h"
#include "gpu/TextureSampler.h"

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
   * If we are instantiated and have a texture, return the mipmap state of that texture. Otherwise,
   * returns the proxy's mipmap state from creation time.
   */
  virtual bool hasMipmaps() const;

  /**
   * Returns the Texture of the proxy. Returns nullptr if the proxy is not instantiated yet.
   */
  std::shared_ptr<Texture> getTexture() const;

  /**
   * Returns true if the backing texture is instantiated.
   */
  bool isInstantiated() const;

  /**
   * Instantiates the backing texture, if necessary. Returns true if the backing texture is
   * instantiated.
   */
  bool instantiate();

  /**
   * Assigns a UniqueKey to the proxy. The proxy will be findable via this UniqueKey using
   * ProxyProvider.findProxyByUniqueKey(). If the updateTextureKey is true, it will also assign the
   * UniqueKey to the target texture.
   */
  void assignUniqueKey(const UniqueKey& uniqueKey, bool updateTextureKey = true);

  /*
   * Removes the UniqueKey from the proxy. If the updateTextureKey is true, it will also remove
   * the UniqueKey from the target texture.
   */
  void removeUniqueKey(bool updateTextureKey = true);

 protected:
  ProxyProvider* provider = nullptr;
  bool setTextureUniqueKey = true;
  std::shared_ptr<Texture> texture = nullptr;

  explicit TextureProxy(ProxyProvider* provider, std::shared_ptr<Texture> texture = nullptr);

  /**
   * Overrides to create a new Texture associated with specified context.
   */
  virtual std::shared_ptr<Texture> onMakeTexture(Context* context);

 private:
  UniqueKey uniqueKey = {};
  std::weak_ptr<TextureProxy> weakThis;

  friend class ProxyProvider;
};
}  // namespace tgfx
