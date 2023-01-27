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

#include "EncodedSource.h"
#include "AsyncImageBuffer.h"
#include "AsyncSource.h"

namespace tgfx {
EncodedSource::EncodedSource(std::shared_ptr<ImageGenerator> generator, bool mipMapped)
    : generator(std::move(generator)), mipMapped(mipMapped) {
}

std::shared_ptr<ImageSource> EncodedSource::onMakeDecodedSource(Context* context) const {
  if (context != nullptr) {
    auto texture =
        std::static_pointer_cast<Texture>(context->resourceCache()->findResourceByOwner(this));
    if (texture != nullptr) {
      return ImageSource::MakeFromTexture(texture);
    }
  }
  auto encodedSource = std::static_pointer_cast<EncodedSource>(weakThis.lock());
  return std::shared_ptr<AsyncSource>(new AsyncSource(std::move(encodedSource)));
}

std::shared_ptr<TextureProxy> EncodedSource::onMakeTextureProxy(Context* context) const {
  return context->proxyProvider()->createTextureProxy(makeAsyncBuffer(), mipMapped);
}

std::shared_ptr<ImageBuffer> EncodedSource::makeAsyncBuffer() const {
  if (generator->asyncSupport()) {
    return generator->makeBuffer();
  } else {
    return AsyncImageBuffer::MakeFrom(generator);
  }
}
}  // namespace tgfx
