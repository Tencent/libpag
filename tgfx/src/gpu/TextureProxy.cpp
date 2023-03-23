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
  if (!uniqueKey.empty()) {
    provider->proxyOwnerMap.erase(uniqueKey.uniqueID());
  }
}

int TextureProxy::width() const {
  return texture->width();
}

int TextureProxy::height() const {
  return texture->height();
}

bool TextureProxy::hasMipmaps() const {
  return texture->getSampler()->hasMipmaps();
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
  if (texture != nullptr && setTextureUniqueKey) {
    texture->assignUniqueKey(uniqueKey);
  }
  return texture != nullptr;
}

void TextureProxy::assignUniqueKey(const UniqueKey& newKey, bool updateTextureKey) {
  if (newKey.empty()) {
    removeUniqueKey(updateTextureKey);
    return;
  }
  if (newKey != uniqueKey) {
    provider->changeUniqueKey(this, newKey);
  }
  setTextureUniqueKey = updateTextureKey;
  if (setTextureUniqueKey && texture != nullptr) {
    texture->assignUniqueKey(newKey);
  }
}

void TextureProxy::removeUniqueKey(bool updateTextureKey) {
  if (!uniqueKey.empty()) {
    provider->removeUniqueKey(this);
  }
  if (updateTextureKey && texture != nullptr) {
    texture->removeUniqueKey();
  }
}

std::shared_ptr<Texture> TextureProxy::onMakeTexture(Context*) {
  return nullptr;
}
}  // namespace tgfx