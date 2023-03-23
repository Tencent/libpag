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

#include "TextureSource.h"

namespace tgfx {
BackendTexture TextureSource::getBackendTexture() const {
  if (!texture->hasUniqueKey(uniqueKey)) {
    return {};
  }
  return texture->getBackendTexture();
}

std::shared_ptr<ImageSource> TextureSource::makeTextureSource(Context* context) const {
  if (texture->getContext() == context && texture->hasUniqueKey(uniqueKey)) {
    return std::static_pointer_cast<ImageSource>(weakThis.lock());
  }
  return nullptr;
}

std::shared_ptr<TextureProxy> TextureSource::onMakeTextureProxy(Context* context, uint32_t) const {
  if (!texture->hasUniqueKey(uniqueKey)) {
    return {};
  }
  return context->proxyProvider()->wrapTexture(texture);
}
}  // namespace tgfx