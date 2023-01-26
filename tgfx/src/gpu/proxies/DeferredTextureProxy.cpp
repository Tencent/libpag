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

#include "DeferredTextureProxy.h"

namespace tgfx {
DeferredTextureProxy::DeferredTextureProxy(ProxyProvider* provider, int width, int height,
                                           PixelFormat format, ImageOrigin origin, bool mipMapped)
    : CacheOwnerTextureProxy(provider),
      _width(width),
      _height(height),
      format(format),
      origin(origin),
      mipMapped(mipMapped) {
}

std::shared_ptr<Texture> DeferredTextureProxy::onMakeTexture(Context* context) {
  if (context == nullptr) {
    return nullptr;
  }
  return Texture::MakeFormat(context, width(), height(), format, origin, mipMapped);
}
}  // namespace tgfx
