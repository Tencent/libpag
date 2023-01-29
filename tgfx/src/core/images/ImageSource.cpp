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

#include "ImageSource.h"
#include "BufferSource.h"
#include "EncodedSource.h"
#include "TextureSource.h"

namespace tgfx {

std::shared_ptr<ImageSource> ImageSource::MakeFromGenerator(
    std::shared_ptr<ImageGenerator> generator, bool mipMapped) {
  if (generator == nullptr) {
    return nullptr;
  }
  auto source = std::shared_ptr<EncodedSource>(new EncodedSource(std::move(generator), mipMapped));
  source->weakThis = source;
  return source;
}

std::shared_ptr<ImageSource> ImageSource::MakeFromBuffer(std::shared_ptr<ImageBuffer> buffer,
                                                         bool mipMapped) {
  if (buffer == nullptr) {
    return nullptr;
  }
  auto source = std::shared_ptr<BufferSource>(new BufferSource(std::move(buffer), mipMapped));
  source->weakThis = source;
  return source;
}

std::shared_ptr<ImageSource> ImageSource::MakeFromTexture(std::shared_ptr<Texture> texture) {
  if (texture == nullptr) {
    return nullptr;
  }
  auto source = std::shared_ptr<TextureSource>(new TextureSource(std::move(texture)));
  source->weakThis = source;
  return source;
}

std::shared_ptr<ImageSource> ImageSource::makeDecodedSource(Context* context) const {
  if (!isLazyGenerated()) {
    return nullptr;
  }
  auto source = onMakeDecodedSource(context);
  if (source != nullptr) {
    source->weakThis = source;
  }
  return source;
}

std::shared_ptr<ImageSource> ImageSource::onMakeDecodedSource(Context*) const {
  return nullptr;
}

std::shared_ptr<ImageSource> ImageSource::makeTextureSource(Context* context,
                                                            bool wrapCacheOnly) const {
  auto texture = getTexture();
  if (texture != nullptr) {
    if (texture->getContext() == context) {
      return std::static_pointer_cast<ImageSource>(weakThis.lock());
    }
    return nullptr;
  }
  auto resourceCache = context->resourceCache();
  texture = std::static_pointer_cast<Texture>(resourceCache->findResourceByOwner(this));
  if (texture != nullptr) {
    return MakeFromTexture(texture);
  }
  if (wrapCacheOnly) {
    return nullptr;
  }
  auto proxy = lockTextureProxy(context);
  if (proxy == nullptr) {
    return nullptr;
  }
  if (!proxy->isInstantiated()) {
    proxy->instantiate();
  }
  return MakeFromTexture(proxy->getTexture());
}

std::shared_ptr<TextureProxy> ImageSource::lockTextureProxy(Context* context) const {
  if (context == nullptr) {
    return nullptr;
  }
  auto provider = context->proxyProvider();
  auto proxy = provider->findProxyByOwner(this);
  if (proxy != nullptr) {
    return proxy;
  }
  proxy = onMakeTextureProxy(context);
  if (proxy != nullptr) {
    proxy->assignCacheOwner(this);
  }
  return proxy;
}
}  // namespace tgfx
