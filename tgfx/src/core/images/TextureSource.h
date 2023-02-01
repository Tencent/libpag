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

namespace tgfx {
/**
 * TextureSource wraps an existing texture.
 */
class TextureSource : public ImageSource {
 public:
  int width() const override {
    return proxy->width();
  }

  int height() const override {
    return proxy->height();
  }

  bool hasMipmaps() const override {
    return proxy->hasMipmaps();
  }

  bool isAlphaOnly() const override;

  bool isTextureBacked() const override {
    return true;
  }

  std::shared_ptr<Texture> getTexture() const override {
    return proxy->getTexture();
  }

  std::shared_ptr<TextureProxy> lockTextureProxy(Context* context) const override;

 protected:
  std::shared_ptr<ImageSource> onMakeMipMapped() const override;

  std::shared_ptr<TextureProxy> onMakeTextureProxy(Context*) const override;

 private:
  std::shared_ptr<TextureProxy> proxy = nullptr;

  explicit TextureSource(std::shared_ptr<Texture> texture);

  friend class ImageSource;
};
}  // namespace tgfx
