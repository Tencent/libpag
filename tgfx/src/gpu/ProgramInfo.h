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

#pragma once

#include "gpu/Blend.h"
#include "gpu/Program.h"
#include "gpu/SamplerState.h"
#include "gpu/TextureSampler.h"
#include "gpu/UniformBuffer.h"

namespace tgfx {
struct SamplerInfo {
  const TextureSampler* sampler;
  SamplerState state;
};
/**
 * This immutable object contains information needed to build a shader program and set API state for
 * a draw.
 */
class ProgramInfo {
 public:
  virtual ~ProgramInfo() = default;

  /**
   * Returns true if the draw requires a texture barrier.
   */
  virtual bool requiresBarrier() const = 0;

  /**
   * Returns the blend info for the draw. A nullptr is returned if the draw does not require
   * blending.
   */
  virtual const BlendInfo* blendInfo() const = 0;

  /**
   * Collects uniform data for the draw.
   */
  virtual void getUniforms(UniformBuffer* uniformBuffer) const = 0;

  /**
   * Collects texture samplers for the draw.
   */
  virtual std::vector<SamplerInfo> getSamplers() const = 0;

  /**
   * Computes a unique key for the program.
   */
  virtual void computeUniqueKey(Context* context, BytesKey* uniqueKey) const = 0;

  /**
   * Creates a new program.
   */
  virtual std::unique_ptr<Program> createProgram(Context* context) const = 0;
};
}  // namespace tgfx
