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

#include "AsyncSource.h"
#include "BufferSource.h"

namespace tgfx {
AsyncSource::AsyncSource(std::shared_ptr<EncodedSource> source) : encodedSource(std::move(source)) {
  imageBuffer = encodedSource->makeAsyncBuffer();
}

std::shared_ptr<TextureProxy> AsyncSource::lockTextureProxy(Context* context) const {
  if (context == nullptr) {
    return nullptr;
  }
  auto provider = context->proxyProvider();
  auto proxy = provider->findProxyByOwner(encodedSource.get());
  if (proxy != nullptr) {
    return proxy;
  }
  proxy = onMakeTextureProxy(context);
  if (proxy != nullptr) {
    proxy->assignCacheOwner(encodedSource.get());
  }
  return proxy;
}

std::shared_ptr<ImageSource> AsyncSource::onMakeMipMapped() const {
  return std::shared_ptr<BufferSource>(new BufferSource(imageBuffer, true));
}

std::shared_ptr<TextureProxy> AsyncSource::onMakeTextureProxy(Context* context) const {
  return context->proxyProvider()->createTextureProxy(imageBuffer, encodedSource->hasMipmaps());
}
}  // namespace tgfx
