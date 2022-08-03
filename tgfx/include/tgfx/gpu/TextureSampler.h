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

#include "tgfx/core/BytesKey.h"
#include "tgfx/gpu/Context.h"
#include "tgfx/gpu/PixelFormat.h"

namespace tgfx {
enum class TextureType { Unknown, TwoD, Rectangle, External };

/**
 * TextureSampler stores the sampling parameters for a backend texture uint.
 */
class TextureSampler {
 public:
  virtual ~TextureSampler() = default;

  /**
   * The pixel format of the sampler.
   */
  PixelFormat format = PixelFormat::RGBA_8888;

  virtual TextureType type() const {
    return TextureType::TwoD;
  }

 protected:
  virtual void computeKey(Context* context, BytesKey* bytesKey) const = 0;

  friend class FragmentProcessor;

  friend class Pipeline;
};
}  // namespace tgfx
