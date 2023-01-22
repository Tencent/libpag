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

#include "tgfx/core/Image.h"
#include "core/images/TextureImage.h"
#include "core/utils/UniqueID.h"
#include "gpu/ProxyProvider.h"
#include "gpu/TextureProxy.h"

namespace tgfx {
std::shared_ptr<Image> Image::MakeFromEncoded(const std::string&) {
  return nullptr;
}

std::shared_ptr<Image> Image::MakeFromEncoded(std::shared_ptr<Data>) {
  return nullptr;
}

std::shared_ptr<Image> Image::MakeFromGenerator(std::unique_ptr<ImageGenerator>) {
  return nullptr;
}

std::shared_ptr<Image> Image::MakeFromPixels(const ImageInfo&, const void*) {
  return nullptr;
}

std::shared_ptr<Image> Image::MakeFromBuffer(std::shared_ptr<ImageBuffer>) {
  return nullptr;
}

std::shared_ptr<Image> Image::MakeFromTexture(std::shared_ptr<Texture> texture) {
  if (texture == nullptr) {
    return nullptr;
  }
  auto image = std::make_shared<TextureImage>(std::move(texture));
  image->weakThis = image;
  return image;
}

bool Image::isLazyGenerated() const {
  return false;
}

bool Image::isTextureBacked() const {
  return false;
}

std::shared_ptr<Image> Image::makeTextureImage(Context* context) const {
  if (context == nullptr) {
    return nullptr;
  }
  return onMakeTextureImage(context);
}

std::shared_ptr<TextureProxy> Image::lockTextureProxy(Context* context) const {
  if (context == nullptr) {
    return nullptr;
  }
  auto provider = context->proxyProvider();
  auto proxy = provider->findProxyByContentOwner(this);
  if (proxy != nullptr) {
    return proxy;
  }
  proxy = onMakeTextureProxy(context);
  if (proxy != nullptr) {
    provider->setContentOwner(proxy, this);
  }
  return proxy;
}
}  // namespace tgfx
