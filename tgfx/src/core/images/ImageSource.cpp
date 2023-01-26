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
#include "core/utils/Log.h"
#include "gpu/ProxyProvider.h"
#include "tgfx/gpu/TextureSampler.h"

namespace tgfx {
class EncodedImageSource : public ImageSource {
 public:
  EncodedImageSource(std::shared_ptr<ImageGenerator> generator, bool mipMapped)
      : imageGenerator(std::move(generator)), mipMapped(mipMapped) {
  }

  int width() const override {
    return imageGenerator->width();
  }

  int height() const override {
    return imageGenerator->height();
  }

  bool hasMipmaps() const override {
    return mipMapped;
  }

  bool isAlphaOnly() const override {
    return imageGenerator->isAlphaOnly();
  }

  bool isLazyGenerated() const override {
    return true;
  }

 protected:
  std::shared_ptr<TextureProxy> onMakeTextureProxy(Context* context) const override {
    auto imageBuffer = imageGenerator->makeBuffer();
    return context->proxyProvider()->createTextureProxy(imageBuffer, mipMapped);
  }

 private:
  std::shared_ptr<ImageGenerator> imageGenerator = nullptr;
  bool mipMapped = false;
};

class RasterImageSource : public ImageSource {
 public:
  RasterImageSource(std::shared_ptr<ImageBuffer> buffer, bool mipMapped)
      : imageBuffer(std::move(buffer)), mipMapped(mipMapped) {
  }

  int width() const override {
    return imageBuffer->width();
  }

  int height() const override {
    return imageBuffer->height();
  }

  bool hasMipmaps() const override {
    return mipMapped;
  }

  bool isAlphaOnly() const override {
    return imageBuffer->isAlphaOnly();
  }

 protected:
  std::shared_ptr<TextureProxy> onMakeTextureProxy(Context* context) const override {
    return context->proxyProvider()->createTextureProxy(imageBuffer, mipMapped);
  }

 private:
  std::shared_ptr<ImageBuffer> imageBuffer = nullptr;
  bool mipMapped = false;
};

class TextureImageSource : public ImageSource {
 public:
  explicit TextureImageSource(std::shared_ptr<Texture> texture)
      : proxy(TextureProxy::Wrap(std::move(texture))) {
  }

  int width() const override {
    return proxy->width();
  }

  int height() const override {
    return proxy->height();
  }

  bool hasMipmaps() const override {
    return proxy->hasMipmaps();
  }

  bool isAlphaOnly() const override {
    auto texture = proxy->getTexture();
    DEBUG_ASSERT(texture != nullptr);
    return texture->getSampler()->format == PixelFormat::ALPHA_8;
  }

  bool isTextureBacked() const override {
    return true;
  }

  std::shared_ptr<Texture> getTexture() const override {
    return proxy->getTexture();
  }

  std::shared_ptr<TextureProxy> lockTextureProxy(Context* context) const override {
    auto texture = proxy->getTexture();
    DEBUG_ASSERT(texture != nullptr);
    if (texture->getContext() != context) {
      return nullptr;
    }
    return proxy;
  }

 protected:
  std::shared_ptr<TextureProxy> onMakeTextureProxy(Context*) const override {
    return nullptr;
  }

 private:
  std::shared_ptr<TextureProxy> proxy = nullptr;
};

std::shared_ptr<ImageSource> ImageSource::MakeFromGenerator(
    std::shared_ptr<ImageGenerator> generator, bool mipMapped) {
  if (generator == nullptr) {
    return nullptr;
  }
  auto source = std::make_shared<EncodedImageSource>(std::move(generator), mipMapped);
  source->weakThis = source;
  return source;
}

std::shared_ptr<ImageSource> ImageSource::MakeFromBuffer(std::shared_ptr<ImageBuffer> buffer,
                                                         bool mipMapped) {
  if (buffer == nullptr) {
    return nullptr;
  }
  auto source = std::make_shared<RasterImageSource>(std::move(buffer), mipMapped);
  source->weakThis = source;
  return source;
}

std::shared_ptr<ImageSource> ImageSource::MakeFromTexture(std::shared_ptr<Texture> texture) {
  if (texture == nullptr) {
    return nullptr;
  }
  auto source = std::make_shared<TextureImageSource>(std::move(texture));
  source->weakThis = source;
  return source;
}

std::shared_ptr<ImageSource> ImageSource::makeTextureSource(Context* context) const {
  auto texture = getTexture();
  if (texture != nullptr && texture->getContext() == context) {
    return std::static_pointer_cast<ImageSource>(weakThis.lock());
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
