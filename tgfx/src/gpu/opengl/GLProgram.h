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

#include <optional>
#include "GLContext.h"
#include "GLProgramDataManager.h"
#include "GLUniformHandler.h"
#include "gpu/GLFragmentProcessor.h"
#include "gpu/GLGeometryProcessor.h"
#include "gpu/GLXferProcessor.h"
#include "gpu/Program.h"
#include "tgfx/gpu/opengl/GLRenderTarget.h"

namespace tgfx {
class GLProgram : public Program {
 public:
  struct Attribute {
    ShaderVar::Type gpuType = ShaderVar::Type::Float;
    size_t offset = 0;
    int location = 0;
  };

  GLProgram(Context* context, BuiltinUniformHandles builtinUniformHandles, unsigned programID,
            const std::vector<Uniform>& uniforms,
            std::unique_ptr<GLGeometryProcessor> geometryProcessor,
            std::unique_ptr<GLXferProcessor> xferProcessor,
            std::vector<std::unique_ptr<GLFragmentProcessor>> fragmentProcessors,
            std::vector<Attribute> attributes, int vertexStride);

  void setupSamplerUniforms(const std::vector<Uniform>& textureSamplers) const;

  /**
   * Gets the GL program ID for this program.
   */
  unsigned programID() const {
    return programId;
  }

  /**
   * This function uploads uniforms, calls each GL*Processor's setData. It binds all fragment
   * processor textures.
   *
   * It is the caller's responsibility to ensure the program is bound before calling.
   */
  void updateUniformsAndTextureBindings(const GLRenderTarget* renderTarget,
                                        const GeometryProcessor& geometryProcessor,
                                        const Pipeline& pipeline);

  int vertexStride() const {
    return _vertexStride;
  }

  const std::vector<Attribute>& vertexAttributes() const {
    return attributes;
  }

 private:
  struct RenderTargetState {
    std::optional<int> width;
    std::optional<int> height;
    std::optional<ImageOrigin> origin;
  };

  // A helper to loop over effects, set the transforms (via subclass) and bind textures
  void setFragmentData(const GLProgramDataManager& programDataManager, const Pipeline& pipeline,
                       int* nextTexSamplerIdx);

  void setRenderTargetState(const GLProgramDataManager& programDataManager,
                            const GLRenderTarget* renderTarget);

  void onReleaseGPU() override;

  RenderTargetState renderTargetState;
  BuiltinUniformHandles builtinUniformHandles;
  unsigned programId = 0;

  // the installed effects
  std::unique_ptr<GLGeometryProcessor> glGeometryProcessor;
  std::unique_ptr<GLXferProcessor> glXferProcessor;
  std::vector<std::unique_ptr<GLFragmentProcessor>> glFragmentProcessors;
  std::vector<Attribute> attributes;
  int _vertexStride = 0;
  std::vector<int> uniformLocations;
};
}  // namespace tgfx
