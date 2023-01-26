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

#include "TextureProxy.h"

namespace tgfx {
class ProxyProvider;

/**
 * The base class for all texture proxies created from a ProxyProvider. This class and its
 * subclasses are not thread-safe, they can be accessed only when the associated context is locked.
 */
class CacheOwnerTextureProxy : public TextureProxy {
 public:
  ~CacheOwnerTextureProxy() override;

  bool instantiate() override;

  void assignCacheOwner(const Cacheable* owner) override;

  void removeCacheOwner() override;

 protected:
  explicit CacheOwnerTextureProxy(ProxyProvider* provider,
                                  std::shared_ptr<Texture> texture = nullptr);

  /**
   * Overrides to create a new Texture associated with specified context.
   */
  virtual std::shared_ptr<Texture> onMakeTexture(Context* context);

 private:
  ProxyProvider* provider = nullptr;
  std::shared_ptr<Cacheable> cacheOwner = nullptr;
  std::weak_ptr<CacheOwnerTextureProxy> weakThis;

  friend class ProxyProvider;
};
}  // namespace tgfx
