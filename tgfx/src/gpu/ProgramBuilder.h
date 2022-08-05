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

#include "FragmentShaderBuilder.h"
#include "GeometryProcessor.h"
#include "Pipeline.h"
#include "UniformHandler.h"
#include "VaryingHandler.h"
#include "VertexShaderBuilder.h"

namespace tgfx {
class ProgramBuilder {
 public:
  virtual ~ProgramBuilder() = default;

  Context* getContext() const {
    return context;
  }

  virtual std::string versionDeclString() = 0;

  virtual std::string textureFuncName() const = 0;

  virtual std::string getShaderVarDeclarations(const ShaderVar& var, ShaderFlags flag) const = 0;

  std::string getUniformDeclarations(ShaderFlags visibility) const;

  const ShaderVar& samplerVariable(SamplerHandle handle) const {
    return uniformHandler()->samplerVariable(handle);
  }

  Swizzle samplerSwizzle(SamplerHandle handle) const {
    return uniformHandler()->samplerSwizzle(handle);
  }

  /**
   * Generates a name for a variable. The generated string will be named prefixed by the prefix
   * char (unless the prefix is '\0'). It also will mangle the name to be stage-specific unless
   * explicitly asked not to.
   */
  std::string nameVariable(char prefix, const std::string& name, bool mangle = true) const;

  virtual UniformHandler* uniformHandler() = 0;

  virtual const UniformHandler* uniformHandler() const = 0;

  virtual VaryingHandler* varyingHandler() = 0;

  virtual VertexShaderBuilder* vertexShaderBuilder() = 0;

  virtual FragmentShaderBuilder* fragmentShaderBuilder() = 0;

 protected:
  Context* context = nullptr;

  ProgramBuilder(Context* context, const GeometryProcessor* geometryProcessor,
                 const Pipeline* pipeline);

  bool emitAndInstallProcessors();

  void finalizeShaders();

  virtual bool checkSamplerCounts() = 0;

  const GeometryProcessor* geometryProcessor = nullptr;
  std::unique_ptr<GLGeometryProcessor> glGeometryProcessor;
  std::unique_ptr<GLXferProcessor> xferProcessor;
  std::vector<std::unique_ptr<GLFragmentProcessor>> fragmentProcessors;
  int numFragmentSamplers = 0;
  BuiltinUniformHandles uniformHandles;

 private:
  /**
   * advanceStage is called by program creator between each processor's emit code. It increments
   * the stage offset for variable name mangling, and also ensures verification variables in the
   * fragment shader are cleared.
   */
  void advanceStage();

  /**
   * Generates a possibly mangled name for a stage variable and writes it to the fragment shader.
   */
  void nameExpression(std::string* output, const std::string& baseName);

  void emitAndInstallGeoProc(std::string* outputColor, std::string* outputCoverage);

  void emitAndInstallFragProcessors(std::string* color, std::string* coverage);

  std::string emitAndInstallFragProc(
      const FragmentProcessor* processor, size_t transformedCoordVarsIdx, const std::string& input,
      std::vector<std::unique_ptr<GLFragmentProcessor>>* glslFragmentProcessors);

  void emitAndInstallXferProc(const std::string& colorIn, const std::string& coverageIn);

  SamplerHandle emitSampler(const TextureSampler* sampler, const std::string& name);

  void emitFSOutputSwizzle();

  int stageIndex = -1;
  const Pipeline* pipeline = nullptr;
  std::vector<ShaderVar> transformedCoordVars = {};
};
}  // namespace tgfx
