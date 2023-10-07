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
#include "gpu/Pipeline.h"

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
                                                 const Pipeline* pipeline) {
  // we set the textures, and uniforms for installed processors in a generic way.

  // We must bind to texture units in the same order in which we set the uniforms in
  // GLProgramDataManager. That is, we bind textures for processors in this order:
  // geometryProcessor, fragmentProcessors, XferProcessor.
  uniformBuffer->advanceStage();
  setRenderTargetState(renderTarget);
  FragmentProcessor::CoordTransformIter coordTransformIter(pipeline);
  auto gp = pipeline->getGeometryProcessor();
  gp->setData(uniformBuffer.get(), &coordTransformIter);
  int nextTexSamplerIdx = 0;
  setFragmentData(pipeline, &nextTexSamplerIdx);

  auto offset = Point::Zero();
  const auto* dstTexture = pipeline->getDstTexture(&offset);
  if (dstTexture) {
    uniformBuffer->advanceStage();
    auto xferProcessor = pipeline->getXferProcessor();
    xferProcessor->setData(uniformBuffer.get(), dstTexture, offset);
    static_cast<GLGpu*>(context->gpu())->bindTexture(nextTexSamplerIdx++, dstTexture->getSampler());
  }
  uniformBuffer->resetStateAndUpload(context);
}

void GLProgram::setFragmentData(const Pipeline* pipeline, int* nextTexSamplerIdx) {
  for (size_t index = 0; index < pipeline->numFragmentProcessors(); ++index) {
    uniformBuffer->advanceStage();
    const auto* currentFP = pipeline->getFragmentProcessor(index);
    FragmentProcessor::Iter iter(currentFP);
    const FragmentProcessor* fp = iter.next();
    while (fp) {
      fp->setData(uniformBuffer.get());
      for (size_t i = 0; i < fp->numTextureSamplers(); ++i) {
        static_cast<GLGpu*>(context->gpu())
            ->bindTexture((*nextTexSamplerIdx)++, fp->textureSampler(i), fp->samplerState(i));
      }
      fp = iter.next();
    }
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
