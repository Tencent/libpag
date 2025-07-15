/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "TextureDrawable.h"

namespace pag {
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
    : device(std::move(device)), texture(texture), origin(origin) {
}

std::shared_ptr<tgfx::Surface> TextureDrawable::onCreateSurface(tgfx::Context* context) {
  return tgfx::Surface::MakeFrom(context, texture, origin);
}
}  // namespace pag
