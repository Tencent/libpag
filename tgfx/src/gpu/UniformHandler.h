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

#include "ResourceHandle.h"
#include "ShaderVar.h"
#include "Swizzle.h"
#include "tgfx/gpu/TextureSampler.h"

namespace tgfx {
static constexpr char NO_MANGLE_PREFIX[] = "tgfx_";

class ProgramBuilder;

class UniformHandler {
 public:
  virtual ~UniformHandler() = default;

  /**
   * Add a uniform variable to the current program, that has visibility in one or more shaders.
   * visibility is a bitfield of ShaderFlag values indicating from which shaders the uniform should
   * be accessible. At least one bit must be set. The actual uniform name will be mangled. If
   * outName is not nullptr then it will refer to the final uniform name after return.
   */
  UniformHandle addUniform(ShaderFlags visibility, ShaderVar::Type type, const std::string& name,
                           std::string* outName = nullptr) {
    bool mangle = name.find(NO_MANGLE_PREFIX) != 0;
    return internalAddUniform(visibility, type, name, mangle, outName);
  }

 protected:
  explicit UniformHandler(ProgramBuilder* program) : programBuilder(program) {
  }

  // This is not owned by the class
  ProgramBuilder* programBuilder;

 private:
  virtual const ShaderVar& samplerVariable(SamplerHandle samplerHandle) const = 0;

  virtual const Swizzle& samplerSwizzle(SamplerHandle samplerHandle) const = 0;

  virtual SamplerHandle addSampler(const TextureSampler* sampler, const std::string& name) = 0;

  virtual UniformHandle internalAddUniform(ShaderFlags visibility, ShaderVar::Type type,
                                           const std::string& name, bool mangleName,
                                           std::string* outName) = 0;

  virtual std::string getUniformDeclarations(ShaderFlags visibility) const = 0;

  friend class ProgramBuilder;
};
}  // namespace tgfx
