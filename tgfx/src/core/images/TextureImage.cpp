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

#include "TextureImage.h"
#include "gpu/ProxyProvider.h"

namespace tgfx {
TextureImage::TextureImage(std::shared_ptr<Texture> texture) : texture(std::move(texture)) {
}

std::shared_ptr<Image> TextureImage::onMakeTextureImage(Context* context) const {
  if (texture->getContext() != context) {
    return nullptr;
  }
  return std::static_pointer_cast<Image>(weakThis.lock());
}

std::shared_ptr<TextureProxy> TextureImage::onMakeTextureProxy(Context* context) const {
  if (texture->getContext() != context) {
    return nullptr;
  }
  return context->proxyProvider()->wrapTexture(texture);
}
}  // namespace tgfx
