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

namespace tgfx {
class ImageBufferTextureProxy : public TextureProxy {
 public:
  ImageBufferTextureProxy(ProxyProvider* provider, std::shared_ptr<ImageBuffer> imageBuffer,
                          bool mipMapped)
      : TextureProxy(provider, imageBuffer->width(), imageBuffer->height()),
        imageBuffer(std::move(imageBuffer)),
        mipMapped(mipMapped) {
  }

 protected:
  std::shared_ptr<Texture> onMakeTexture(Context* context) override {
    if (imageBuffer == nullptr) {
      return nullptr;
    }
    std::shared_ptr<Texture> texture = nullptr;
    if (mipMapped && imageBuffer->mipMapSupport()) {
      texture = imageBuffer->makeMipMappedTexture(context);
    } else {
      texture = imageBuffer->makeTexture(context);
    }
    if (texture != nullptr) {
      imageBuffer = nullptr;
    }
    return texture;
  }

 private:
  std::shared_ptr<ImageBuffer> imageBuffer = nullptr;
  bool mipMapped = false;
};

class LazyTextureProxy : public TextureProxy {
 public:
  LazyTextureProxy(ProxyProvider* provider, int width, int height, PixelFormat format,
                   ImageOrigin origin, bool mipMapped)
      : TextureProxy(provider, width, height),
        format(format),
        origin(origin),
        mipMapped(mipMapped) {
  }

 protected:
  std::shared_ptr<Texture> onMakeTexture(Context* context) override {
    if (context == nullptr) {
      return nullptr;
    }
    return Texture::MakeFormat(context, width(), height(), format, origin, mipMapped);
  }

 private:
  PixelFormat format = PixelFormat::RGBA_8888;
  ImageOrigin origin = ImageOrigin::TopLeft;
  bool mipMapped = false;
};

ProxyProvider::ProxyProvider(Context* context) : context(context) {
}

void ProxyProvider::setContentOwner(std::shared_ptr<TextureProxy> proxy, const Cacheable* owner) {
  if (proxy == nullptr || proxy->contentOwner.get() == owner) {
    return;
  }
  if (owner == nullptr) {
    removeContentOwner(proxy);
    return;
  }
  auto result = proxyMap.find(owner->uniqueID());
  if (result != proxyMap.end()) {
    removeContentOwner(result->second.lock());
  }
  if (proxy->contentOwner) {
    proxyMap.erase(proxy->contentOwner->uniqueID());
  }
  proxy->contentOwner = owner->weakThis.lock();
  proxyMap[owner->uniqueID()] = proxy;
  if (!proxy->isInstantiated()) {
    return;
  }
  auto texture = proxy->getTexture();
  context->resourceCache()->setContentOwner(texture.get(), owner);
}

void ProxyProvider::removeContentOwner(std::shared_ptr<TextureProxy> proxy) {
  if (proxy == nullptr || proxy->contentOwner == nullptr) {
    return;
  }
  proxyMap.erase(proxy->contentOwner->uniqueID());
  proxy->contentOwner = nullptr;
  if (!proxy->isInstantiated()) {
    return;
  }
  auto texture = proxy->getTexture();
  context->resourceCache()->removeContentOwner(texture.get());
}

std::shared_ptr<TextureProxy> ProxyProvider::findProxyByContentOwner(const Cacheable* owner) {
  if (owner == nullptr) {
    return nullptr;
  }
  auto result = proxyMap.find(owner->uniqueID());
  if (result != proxyMap.end()) {
    return result->second.lock();
  }
  auto resourceCache = context->resourceCache();
  auto texture = std::static_pointer_cast<Texture>(resourceCache->getByContentOwner(owner));
  if (texture == nullptr) {
    return nullptr;
  }
  auto proxy = wrapTexture(texture);
  proxy->contentOwner = owner->weakThis.lock();
  proxyMap[owner->uniqueID()] = proxy;
  return nullptr;
}

std::shared_ptr<TextureProxy> ProxyProvider::createTextureProxy(
    std::shared_ptr<ImageBuffer> imageBuffer, bool mipMapped) {
  if (imageBuffer == nullptr) {
    return nullptr;
  }
  return std::make_shared<ImageBufferTextureProxy>(this, std::move(imageBuffer), mipMapped);
}

std::shared_ptr<TextureProxy> ProxyProvider::createTextureProxy(int width, int height,
                                                                PixelFormat format,
                                                                ImageOrigin origin,
                                                                bool mipMapped) {
  if (!Texture::CheckSizeAndFormat(context, width, height, format)) {
    return nullptr;
  }
  return std::make_shared<LazyTextureProxy>(this, width, height, format, origin, mipMapped);
}

std::shared_ptr<TextureProxy> ProxyProvider::wrapTexture(std::shared_ptr<Texture> texture) {
  if (texture == nullptr) {
    return nullptr;
  }
  auto proxy =
      std::shared_ptr<TextureProxy>(new TextureProxy(this, texture->width(), texture->height()));
  proxy->texture = texture;
  return proxy;
}

}  // namespace tgfx
