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
TextureProxy::TextureProxy(ProxyProvider* provider, int width, int height)
    : provider(provider), _width(width), _height(height) {
}

TextureProxy::~TextureProxy() {
  if (contentOwner != nullptr) {
    provider->proxyMap.erase(contentOwner->uniqueID());
  }
}

bool TextureProxy::instantiate() {
  if (texture != nullptr) {
    return true;
  }
  auto context = provider->context;
  texture = onMakeTexture(context);
  if (texture != nullptr && contentOwner != nullptr) {
    context->resourceCache()->setContentOwner(texture.get(), contentOwner.get());
  }
  return texture != nullptr;
}

std::shared_ptr<Texture> TextureProxy::onMakeTexture(Context*) {
  return nullptr;
}
}  // namespace tgfx