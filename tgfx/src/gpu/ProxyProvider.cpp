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
#include "gpu/PlainTexture.h"

namespace tgfx {
class ImageBufferTextureProxy : public TextureProxy {
 public:
  ImageBufferTextureProxy(ProxyProvider* provider, std::shared_ptr<ImageBuffer> imageBuffer,
                          bool mipMapped)
      : TextureProxy(provider), imageBuffer(std::move(imageBuffer)), mipMapped(mipMapped) {
  }

  int width() const override {
    return texture ? texture->width() : imageBuffer->width();
  }

  int height() const override {
    return texture ? texture->height() : imageBuffer->height();
  }

  bool hasMipmaps() const override {
    return texture ? texture->getSampler()->hasMipmaps() : mipMapped;
  }

 protected:
  std::shared_ptr<Texture> onMakeTexture(Context* context) override {
    if (imageBuffer == nullptr) {
      return nullptr;
    }
    auto texture = Texture::MakeFrom(context, imageBuffer, mipMapped);
    if (texture != nullptr) {
      imageBuffer = nullptr;
    }
    return texture;
  }

 private:
  std::shared_ptr<ImageBuffer> imageBuffer = nullptr;
  bool mipMapped = false;
};

class ImageGeneratorTextureProxy : public TextureProxy {
 public:
  ImageGeneratorTextureProxy(ProxyProvider* provider, std::shared_ptr<ImageGeneratorTask> task,
                             bool mipMapped)
      : TextureProxy(provider), task(std::move(task)), mipMapped(mipMapped) {
  }

  int width() const override {
    return texture ? texture->width() : task->imageWidth();
  }

  int height() const override {
    return texture ? texture->height() : task->imageHeight();
  }

  bool hasMipmaps() const override {
    return texture ? texture->getSampler()->hasMipmaps() : mipMapped;
  }

 protected:
  std::shared_ptr<Texture> onMakeTexture(Context* context) override {
    if (task == nullptr) {
      return nullptr;
    }
    auto buffer = task->getBuffer();
    if (buffer == nullptr) {
      return nullptr;
    }
    auto texture = Texture::MakeFrom(context, buffer, mipMapped);
    if (texture != nullptr) {
      task = nullptr;
    }
    return texture;
  }

 private:
  std::shared_ptr<ImageGeneratorTask> task = nullptr;
  bool mipMapped = false;
};

class DeferredTextureProxy : public TextureProxy {
 public:
  DeferredTextureProxy(ProxyProvider* provider, int width, int height, PixelFormat format,
                       ImageOrigin origin, bool mipMapped)
      : TextureProxy(provider),
        _width(width),
        _height(height),
        format(format),
        origin(origin),
        mipMapped(mipMapped) {
  }

  int width() const override {
    return _width;
  }

  int height() const override {
    return _height;
  }

  bool hasMipmaps() const override {
    return texture ? texture->getSampler()->hasMipmaps() : mipMapped;
  }

 protected:
  std::shared_ptr<Texture> onMakeTexture(Context* context) override {
    if (context == nullptr) {
      return nullptr;
    }
    return Texture::MakeFormat(context, width(), height(), format, origin, mipMapped);
  }

 private:
  int _width = 0;
  int _height = 0;
  PixelFormat format = PixelFormat::RGBA_8888;
  ImageOrigin origin = ImageOrigin::TopLeft;
  bool mipMapped = false;
};

ProxyProvider::ProxyProvider(Context* context) : context(context) {
}

std::shared_ptr<TextureProxy> ProxyProvider::findProxyByUniqueKey(const UniqueKey& uniqueKey) {
  if (uniqueKey.empty()) {
    return nullptr;
  }
  auto result = proxyOwnerMap.find(uniqueKey.uniqueID());
  if (result != proxyOwnerMap.end()) {
    return result->second->weakThis.lock();
  }
  auto resourceCache = context->resourceCache();
  auto texture = std::static_pointer_cast<Texture>(resourceCache->findUniqueResource(uniqueKey));
  if (texture == nullptr) {
    return nullptr;
  }
  auto proxy = wrapTexture(texture);
  proxy->assignUniqueKey(uniqueKey, false);
  return proxy;
}

std::shared_ptr<TextureProxy> ProxyProvider::createTextureProxy(
    std::shared_ptr<ImageBuffer> imageBuffer, bool mipMapped) {
  if (imageBuffer == nullptr) {
    return nullptr;
  }
  auto proxy = std::make_shared<ImageBufferTextureProxy>(this, std::move(imageBuffer), mipMapped);
  proxy->weakThis = proxy;
  return proxy;
}

std::shared_ptr<TextureProxy> ProxyProvider::createTextureProxy(
    std::shared_ptr<ImageGenerator> generator, bool mipMapped, bool disableAsyncTask) {
  if (generator == nullptr) {
    return nullptr;
  }
  if (disableAsyncTask || generator->asyncSupport()) {
    auto buffer = generator->makeBuffer(!mipMapped);
    return createTextureProxy(std::move(buffer), mipMapped);
  }
  auto task = ImageGeneratorTask::MakeFrom(std::move(generator), !mipMapped);
  return createTextureProxy(std::move(task), mipMapped);
}

std::shared_ptr<TextureProxy> ProxyProvider::createTextureProxy(
    std::shared_ptr<ImageGeneratorTask> task, bool mipMapped) {
  if (task == nullptr) {
    return nullptr;
  }
  auto proxy = std::make_shared<ImageGeneratorTextureProxy>(this, std::move(task), mipMapped);
  proxy->weakThis = proxy;
  return proxy;
}

std::shared_ptr<TextureProxy> ProxyProvider::createTextureProxy(int width, int height,
                                                                PixelFormat format,
                                                                ImageOrigin origin,
                                                                bool mipMapped) {
  if (!PlainTexture::CheckSizeAndFormat(context, width, height, format)) {
    return nullptr;
  }
  auto proxy =
      std::make_shared<DeferredTextureProxy>(this, width, height, format, origin, mipMapped);
  proxy->weakThis = proxy;
  return proxy;
}

std::shared_ptr<TextureProxy> ProxyProvider::wrapTexture(std::shared_ptr<Texture> texture) {
  if (texture == nullptr || texture->getContext() != context) {
    return nullptr;
  }
  auto proxy = std::shared_ptr<TextureProxy>(new TextureProxy(this, std::move(texture)));
  proxy->weakThis = proxy;
  return proxy;
}

void ProxyProvider::changeUniqueKey(TextureProxy* proxy, const UniqueKey& uniqueKey) {
  auto result = proxyOwnerMap.find(uniqueKey.uniqueID());
  if (result != proxyOwnerMap.end()) {
    result->second->removeUniqueKey();
  }
  if (!proxy->uniqueKey.empty()) {
    proxyOwnerMap.erase(proxy->uniqueKey.uniqueID());
  }
  proxy->uniqueKey = uniqueKey;
  proxyOwnerMap[uniqueKey.uniqueID()] = proxy;
}

void ProxyProvider::removeUniqueKey(TextureProxy* proxy) {
  proxyOwnerMap.erase(proxy->uniqueKey.uniqueID());
  proxy->uniqueKey = {};
}

}  // namespace tgfx
