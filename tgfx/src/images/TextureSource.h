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

#pragma once

#include "ImageSource.h"
#include "gpu/TextureSampler.h"

namespace tgfx {
/**
 * TextureSource wraps an existing texture.
 */
class TextureSource : public ImageSource {
 public:
  int width() const override {
    return texture->width();
  }

  int height() const override {
    return texture->height();
  }

  bool hasMipmaps() const override {
    return texture->getSampler()->hasMipmaps();
  }

  bool isAlphaOnly() const override {
    return texture->getSampler()->format == PixelFormat::ALPHA_8;
  }

  bool isTextureBacked() const override {
    return true;
  }

  BackendTexture getBackendTexture() const override {
    return texture->getBackendTexture();
  }

  std::shared_ptr<ImageSource> makeTextureSource(Context* context) const override;

 protected:
  std::shared_ptr<ImageSource> onMakeMipMapped() const override {
    return nullptr;
  }

  std::shared_ptr<TextureProxy> onMakeTextureProxy(Context* context, uint32_t) const override {
    return context->proxyProvider()->wrapTexture(texture);
  }

 private:
  std::shared_ptr<Texture> texture = nullptr;

  explicit TextureSource(UniqueKey uniqueKey, std::shared_ptr<Texture> texture)
      : ImageSource(std::move(uniqueKey)), texture(std::move(texture)) {
  }

  friend class ImageSource;
};
}  // namespace tgfx
