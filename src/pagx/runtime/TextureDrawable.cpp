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

#include "pagx/runtime/TextureDrawable.h"

namespace pagx {

std::shared_ptr<TextureDrawable> TextureDrawable::MakeFrom(std::shared_ptr<tgfx::Device> device,
                                                           const tgfx::BackendTexture& texture,
                                                           tgfx::ImageOrigin origin) {
  if (device == nullptr || !texture.isValid()) {
    return nullptr;
  }
  return std::shared_ptr<TextureDrawable>(new TextureDrawable(std::move(device), texture, origin));
}

TextureDrawable::TextureDrawable(std::shared_ptr<tgfx::Device> device,
                                 const tgfx::BackendTexture& texture, tgfx::ImageOrigin origin)
    : _device(std::move(device)), _texture(texture), _origin(origin) {
}

std::shared_ptr<tgfx::Surface> TextureDrawable::getSurface(tgfx::Context* context) {
  if (surface != nullptr) {
    return surface;
  }
  if (context == nullptr) {
    return nullptr;
  }
  surface = tgfx::Surface::MakeFrom(context, _texture, _origin);
  return surface;
}

}  // namespace pagx
