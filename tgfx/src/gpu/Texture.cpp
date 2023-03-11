/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "gpu/Texture.h"
#include "gpu/TextureSampler.h"

namespace tgfx {
std::shared_ptr<Texture> Texture::MakeFrom(Context* context,
                                           std::shared_ptr<ImageBuffer> imageBuffer,
                                           bool mipMapped) {
  if (context == nullptr || imageBuffer == nullptr) {
    return nullptr;
  }
  return imageBuffer->onMakeTexture(context, mipMapped);
}

Texture::Texture(int width, int height, ImageOrigin origin)
    : _width(width), _height(height), _origin(origin) {
}

Point Texture::getTextureCoord(float x, float y) const {
  if (getSampler()->type() == TextureType::Rectangle) {
    return {x, y};
  }
  return {x / static_cast<float>(width()), y / static_cast<float>(height())};
}

BackendTexture Texture::getBackendTexture() const {
  return getSampler()->getBackendTexture(width(), height());
}
}  // namespace tgfx
