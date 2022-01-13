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

#pragma once

#include "gpu/TextureSampler.h"

namespace pag {
class GLTextureSampler : public TextureSampler {
 public:
  GLTextureSampler() = default;

  GLTextureSampler(PixelConfig config, GLTextureInfo textureInfo)
      : TextureSampler(config), glInfo(textureInfo) {
  }

  void computeKey(Context* context, BytesKey* bytesKey) const override {
    const auto* gl = GLContext::Unwrap(context);
    bytesKey->write(static_cast<uint32_t>(gl->caps->configTextureSwizzle(config).asKey()));
    bytesKey->write(glInfo.target);
  }

  GLTextureInfo glInfo = {};
};
}  // namespace pag
