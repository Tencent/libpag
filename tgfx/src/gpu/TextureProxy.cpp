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

#include "TextureProxy.h"
#include "ProxyProvider.h"

namespace tgfx {
TextureProxy::TextureProxy(ProxyProvider* provider, std::shared_ptr<Texture> texture)
    : provider(provider), texture(std::move(texture)) {
}

TextureProxy::~TextureProxy() {
  if (proxyOwnerID != 0) {
    provider->proxyOwnerMap.erase(proxyOwnerID);
  }
}

int TextureProxy::width() const {
  return texture->width();
}

int TextureProxy::height() const {
  return texture->height();
}

bool TextureProxy::hasMipmaps() const {
  return texture->getSampler()->mipMapped();
}

std::shared_ptr<Texture> TextureProxy::getTexture() const {
  return texture;
}

bool TextureProxy::isInstantiated() const {
  return texture != nullptr;
}

bool TextureProxy::instantiate() {
  if (texture != nullptr) {
    return true;
  }
  texture = onMakeTexture(provider->context);
  if (texture != nullptr && !cacheOwner.expired()) {
    auto owner = cacheOwner.lock();
    texture->assignCacheOwner(owner.get());
  }
  return texture != nullptr;
}

void TextureProxy::assignProxyOwner(const Cacheable* owner, bool updateTextureOwner) {
  if (owner == nullptr) {
    removeProxyOwner(updateTextureOwner);
    return;
  }
  if (proxyOwnerID != owner->uniqueID()) {
    provider->changeProxyOwner(this, owner->uniqueID());
  }
  if (!updateTextureOwner) {
    return;
  }
  cacheOwner = owner->weakThis;
  if (texture != nullptr) {
    texture->assignCacheOwner(owner);
  }
}

void TextureProxy::removeProxyOwner(bool updateTexture) {
  if (proxyOwnerID != 0) {
    provider->removeProxyOwner(this);
  }
  if (!updateTexture) {
    return;
  }
  cacheOwner.reset();
  if (texture != nullptr) {
    texture->removeCacheOwner();
  }
}

std::shared_ptr<Texture> TextureProxy::onMakeTexture(Context*) {
  return nullptr;
}
}  // namespace tgfx