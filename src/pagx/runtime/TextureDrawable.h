/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include "pagx/runtime/Drawable.h"

namespace pagx {

/**
 * TextureDrawable backs a PAGSurface with an externally created backend texture. The device is
 * obtained from GLDevice::Current() so that the PAGSurface shares the same GPU context as the
 * caller. The texture must not be bound to any framebuffer when passed in.
 */
class TextureDrawable : public Drawable {
 public:
  static std::shared_ptr<TextureDrawable> MakeFrom(std::shared_ptr<tgfx::Device> device,
                                                   const tgfx::BackendTexture& texture,
                                                   tgfx::ImageOrigin origin);

  int width() const override {
    return _texture.width();
  }

  int height() const override {
    return _texture.height();
  }

  std::shared_ptr<tgfx::Device> getDevice() override {
    return _device;
  }

  std::shared_ptr<tgfx::Surface> getSurface(tgfx::Context* context) override;

 private:
  TextureDrawable(std::shared_ptr<tgfx::Device> device, const tgfx::BackendTexture& texture,
                  tgfx::ImageOrigin origin);

  std::shared_ptr<tgfx::Device> _device = nullptr;
  tgfx::BackendTexture _texture = {};
  tgfx::ImageOrigin _origin = tgfx::ImageOrigin::TopLeft;
};

}  // namespace pagx
