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

#include "CacheOwnerTextureProxy.h"

namespace tgfx {
class DeferredTextureProxy : public CacheOwnerTextureProxy {
 public:
  int width() const override {
    return _width;
  }

  int height() const override {
    return _height;
  }

  bool hasMipmaps() const override {
    return texture ? texture->getSampler()->mipMapped() : mipMapped;
  }

 protected:
  std::shared_ptr<Texture> onMakeTexture(Context* context) override;

 private:
  int _width = 0;
  int _height = 0;
  PixelFormat format = PixelFormat::RGBA_8888;
  ImageOrigin origin = ImageOrigin::TopLeft;
  bool mipMapped = false;

  DeferredTextureProxy(ProxyProvider* provider, int width, int height, PixelFormat format,
                       ImageOrigin origin, bool mipMapped);

  friend ProxyProvider;
};
}  // namespace tgfx
