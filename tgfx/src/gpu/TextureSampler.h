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

#include "core/BytesKey.h"
#include "gpu/Context.h"
#include "gpu/PixelConfig.h"

namespace tgfx {
class TextureSampler {
 public:
  TextureSampler() = default;

  virtual ~TextureSampler() = default;

  explicit TextureSampler(PixelConfig config) : config(config) {
  }

  virtual void computeKey(Context* context, BytesKey* bytesKey) const = 0;

  PixelConfig config = PixelConfig::RGBA_8888;
};
}  // namespace tgfx
