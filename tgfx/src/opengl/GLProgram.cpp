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

#include "GLProgram.h"
#include "GLGpu.h"
#include "GLUtil.h"

namespace tgfx {
GLProgram::GLProgram(Context* context, unsigned programID,
                     std::unique_ptr<GLUniformBuffer> uniformBuffer,
                     std::vector<Attribute> attributes, int vertexStride)
    : Program(context),
      programId(programID),
      uniformBuffer(std::move(uniformBuffer)),
      attributes(std::move(attributes)),
      _vertexStride(vertexStride) {
}

void GLProgram::setupSamplerUniforms(const std::vector<Uniform>& textureSamplers) const {
  auto gl = GLFunctions::Get(context);
  gl->useProgram(programId);
  // Assign texture units to sampler uniforms one time up front.
  auto count = static_cast<int>(textureSamplers.size());
  for (int i = 0; i < count; ++i) {
    const auto& sampler = textureSamplers[i];
    if (UNUSED_UNIFORM != sampler.location) {
      gl->uniform1i(sampler.location, i);
    }
  }
}

void GLProgram::onReleaseGPU() {
  if (programId) {
    auto gl = GLFunctions::Get(context);
    gl->deleteProgram(programId);
  }
}

void GLProgram::updateUniformsAndTextureBindings(const GLRenderTarget* renderTarget,
                                                 const ProgramInfo* programInfo) {
  setRenderTargetState(renderTarget);
  programInfo->getUniforms(uniformBuffer.get());
  uniformBuffer->uploadToGPU(context);
  auto samplers = programInfo->getSamplers();
  int textureUnit = 0;
  auto gpu = static_cast<GLGpu*>(context->gpu());
  for (auto& info : samplers) {
    gpu->bindTexture(textureUnit++, info.sampler, info.state);
  }
}

static std::array<float, 4> GetRTAdjustArray(int width, int height, bool flipY) {
  std::array<float, 4> result = {};
  result[0] = 2.f / static_cast<float>(width);
  result[2] = 2.f / static_cast<float>(height);
  result[1] = -1.f;
  result[3] = -1.f;
  if (flipY) {
    result[2] = -result[2];
    result[3] = -result[3];
  }
  return result;
}

void GLProgram::setRenderTargetState(const GLRenderTarget* renderTarget) {
  int width = renderTarget->width();
  int height = renderTarget->height();
  auto origin = renderTarget->origin();
  if (renderTargetState.width == width && renderTargetState.height == height &&
      renderTargetState.origin == origin) {
    return;
  }
  renderTargetState.width = width;
  renderTargetState.height = height;
  renderTargetState.origin = origin;
  auto v = GetRTAdjustArray(width, height, origin == ImageOrigin::BottomLeft);
  uniformBuffer->setData(RTAdjustName, v.data());
}
}  // namespace tgfx
