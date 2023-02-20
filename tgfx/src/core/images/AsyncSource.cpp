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

#include "AsyncSource.h"
#include "BufferSource.h"

namespace tgfx {
AsyncSource::AsyncSource(std::shared_ptr<EncodedSource> source) : encodedSource(std::move(source)) {
  auto generator = encodedSource->generator;
  auto tryHardware = !encodedSource->mipMapped;
  if (generator->asyncSupport()) {
    imageBuffer = generator->makeBuffer(tryHardware);
  } else {
    imageTask = ImageGeneratorTask::MakeFrom(generator, tryHardware);
  }
}

const Cacheable* AsyncSource::getCacheOwner() const {
  return encodedSource.get();
}

std::shared_ptr<ImageSource> AsyncSource::onMakeMipMapped() const {
  return encodedSource->onMakeMipMapped();
}

std::shared_ptr<TextureProxy> AsyncSource::onMakeTextureProxy(Context* context) const {
  auto provider = context->proxyProvider();
  if (imageBuffer) {
    return provider->createTextureProxy(imageBuffer, encodedSource->mipMapped);
  }
  return provider->createTextureProxy(imageTask, encodedSource->mipMapped);
}
}  // namespace tgfx
