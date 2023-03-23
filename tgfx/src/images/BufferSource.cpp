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

#include "BufferSource.h"

namespace tgfx {
BufferSource::BufferSource(UniqueKey uniqueKey, std::shared_ptr<ImageBuffer> buffer, bool mipMapped)
    : ImageSource(std::move(uniqueKey)), imageBuffer(std::move(buffer)), mipMapped(mipMapped) {
}

std::shared_ptr<ImageSource> BufferSource::onMakeMipMapped() const {
  return std::shared_ptr<BufferSource>(new BufferSource(UniqueKey::Next(), imageBuffer, true));
}

std::shared_ptr<TextureProxy> BufferSource::onMakeTextureProxy(Context* context, uint32_t) const {
  return context->proxyProvider()->createTextureProxy(imageBuffer, mipMapped);
}
}  // namespace tgfx
