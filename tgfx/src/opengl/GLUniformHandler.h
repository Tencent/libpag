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

#include "gpu/Swizzle.h"
#include "gpu/UniformHandler.h"
#include "opengl/GLUniformBuffer.h"

namespace tgfx {
static constexpr int UNUSED_UNIFORM = -1;

struct Uniform {
  ShaderVar variable;
  ShaderFlags visibility = ShaderFlags::None;
  int location = UNUSED_UNIFORM;
};

class GLUniformHandler : public UniformHandler {
 private:
  explicit GLUniformHandler(ProgramBuilder* program) : UniformHandler(program) {
  }

  std::string internalAddUniform(ShaderFlags visibility, ShaderVar::Type type,
                                 const std::string& name, bool mangleName) override;

  SamplerHandle addSampler(const TextureSampler* sampler, const std::string& name) override;

  const ShaderVar& samplerVariable(SamplerHandle handle) const override {
    return samplers[handle.toIndex()].variable;
  }

  const Swizzle& samplerSwizzle(SamplerHandle handle) const override {
    return samplerSwizzles[handle.toIndex()];
  }

  std::string getUniformDeclarations(ShaderFlags visibility) const override;

  void resolveUniformLocations(unsigned programID);

  std::unique_ptr<GLUniformBuffer> makeUniformBuffer() const;

  std::unordered_map<std::string, Uniform> uniforms;
  std::vector<Uniform> samplers;
  std::vector<Swizzle> samplerSwizzles;

  friend class GLProgramBuilder;
};
}  // namespace tgfx
