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

#include "ProxyProvider.h"
#include "gpu/proxies/DeferredTextureProxy.h"
#include "gpu/proxies/ImageBufferTextureProxy.h"

namespace tgfx {
ProxyProvider::ProxyProvider(Context* context) : context(context) {
}

std::shared_ptr<TextureProxy> ProxyProvider::findProxyByOwner(const Cacheable* owner) {
  if (owner == nullptr) {
    return nullptr;
  }
  auto result = cacheOwnerMap.find(owner->uniqueID());
  if (result != cacheOwnerMap.end()) {
    return result->second->weakThis.lock();
  }
  auto resourceCache = context->resourceCache();
  auto texture = std::static_pointer_cast<Texture>(resourceCache->findResourceByOwner(owner));
  if (texture == nullptr) {
    return nullptr;
  }
  auto proxy = std::static_pointer_cast<CacheOwnerTextureProxy>(wrapTexture(texture));
  proxy->cacheOwner = owner->weakThis.lock();
  cacheOwnerMap[owner->uniqueID()] = proxy.get();
  return proxy;
}

std::shared_ptr<TextureProxy> ProxyProvider::createTextureProxy(
    std::shared_ptr<ImageBuffer> imageBuffer, bool mipMapped) {
  if (imageBuffer == nullptr) {
    return nullptr;
  }
  auto proxy = std::shared_ptr<CacheOwnerTextureProxy>(
      new ImageBufferTextureProxy(this, std::move(imageBuffer), mipMapped));
  proxy->weakThis = proxy;
  return proxy;
}

std::shared_ptr<TextureProxy> ProxyProvider::createTextureProxy(int width, int height,
                                                                PixelFormat format,
                                                                ImageOrigin origin,
                                                                bool mipMapped) {
  if (!Texture::CheckSizeAndFormat(context, width, height, format)) {
    return nullptr;
  }
  auto proxy = std::shared_ptr<CacheOwnerTextureProxy>(
      new DeferredTextureProxy(this, width, height, format, origin, mipMapped));
  proxy->weakThis = proxy;
  return proxy;
}

std::shared_ptr<TextureProxy> ProxyProvider::wrapTexture(std::shared_ptr<Texture> texture) {
  if (texture == nullptr || texture->getContext() != context) {
    return nullptr;
  }
  auto proxy =
      std::shared_ptr<CacheOwnerTextureProxy>(new CacheOwnerTextureProxy(this, std::move(texture)));
  proxy->weakThis = proxy;
  return proxy;
}

void ProxyProvider::changeCacheOwner(CacheOwnerTextureProxy* proxy, const Cacheable* owner) {
  auto result = cacheOwnerMap.find(owner->uniqueID());
  if (result != cacheOwnerMap.end()) {
    result->second->removeCacheOwner();
  }
  if (proxy->cacheOwner) {
    cacheOwnerMap.erase(proxy->cacheOwner->uniqueID());
  }
  proxy->cacheOwner = owner->weakThis.lock();
  cacheOwnerMap[owner->uniqueID()] = proxy;
}

void ProxyProvider::removeCacheOwner(CacheOwnerTextureProxy* proxy) {
  cacheOwnerMap.erase(proxy->cacheOwner->uniqueID());
  proxy->cacheOwner = nullptr;
}

}  // namespace tgfx
