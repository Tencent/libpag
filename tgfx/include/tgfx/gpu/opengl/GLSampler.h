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

#include "tgfx/gpu/TextureSampler.h"

namespace tgfx {
/**
 * Defines the sampling parameters for an OpenGL texture uint.
 */
class GLSampler : public TextureSampler {
 public:
  /**
   * The OpenGL texture id of the sampler.
   */
  unsigned id = 0;

  /**
   * The OpenGL texture target of the sampler.
   */
  unsigned target = 0x0DE1;  // GL_TEXTURE_2D;

  TextureType type() const override;

 protected:
  void computeKey(Context* context, BytesKey* bytesKey) const override;
};
}  // namespace tgfx
