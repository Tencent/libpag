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

#include "CacheOwnerTextureProxy.h"
#include "gpu/ProxyProvider.h"

namespace tgfx {
CacheOwnerTextureProxy::CacheOwnerTextureProxy(ProxyProvider* provider,
                                               std::shared_ptr<Texture> texture)
    : TextureProxy(std::move(texture)), provider(provider) {
}

CacheOwnerTextureProxy::~CacheOwnerTextureProxy() {
  if (cacheOwner != nullptr) {
    provider->cacheOwnerMap.erase(cacheOwner->uniqueID());
  }
}

bool CacheOwnerTextureProxy::instantiate() {
  if (texture != nullptr) {
    return true;
  }
  texture = onMakeTexture(provider->context);
  if (texture != nullptr && cacheOwner != nullptr) {
    texture->assignCacheOwner(cacheOwner.get());
  }
  return texture != nullptr;
}

void CacheOwnerTextureProxy::assignCacheOwner(const Cacheable* owner) {
  if (cacheOwner.get() == owner) {
    return;
  }
  if (owner == nullptr) {
    removeCacheOwner();
    return;
  }
  provider->changeCacheOwner(this, owner);
  if (texture != nullptr) {
    texture->assignCacheOwner(owner);
  }
}

void CacheOwnerTextureProxy::removeCacheOwner() {
  if (cacheOwner == nullptr) {
    return;
  }
  provider->removeCacheOwner(this);
  if (texture != nullptr) {
    texture->removeCacheOwner();
  }
}

std::shared_ptr<Texture> CacheOwnerTextureProxy::onMakeTexture(Context*) {
  return nullptr;
}
}  // namespace tgfx
