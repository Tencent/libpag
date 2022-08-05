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
#include "GLContext.h"
#include "GLUtil.h"
#include "gpu/Pipeline.h"
#include "tgfx/gpu/opengl/GLTexture.h"

namespace tgfx {
GLProgram::GLProgram(Context* context, BuiltinUniformHandles builtinUniformHandles,
                     unsigned programID, const std::vector<Uniform>& uniforms,
                     std::unique_ptr<GLGeometryProcessor> geometryProcessor,
                     std::unique_ptr<GLXferProcessor> xferProcessor,
                     std::vector<std::unique_ptr<GLFragmentProcessor>> fragmentProcessors,
                     std::vector<Attribute> attributes, int vertexStride)
    : Program(context),
      builtinUniformHandles(builtinUniformHandles),
      programId(programID),
      glGeometryProcessor(std::move(geometryProcessor)),
      glXferProcessor(std::move(xferProcessor)),
      glFragmentProcessors(std::move(fragmentProcessors)),
      attributes(std::move(attributes)),
      _vertexStride(vertexStride) {
  for (const auto& uniform : uniforms) {
    uniformLocations.emplace_back(uniform.location);
  }
}

void GLProgram::setupSamplerUniforms(const std::vector<Uniform>& textureSamplers) const {
  auto gl = GLFunctions::Get(context);
  gl->useProgram(programId);
  // Assign texture units to sampler uniforms one time up front.
  auto count = static_cast<int>(textureSamplers.size());
  for (int i = 0; i < count; ++i) {
    const auto& sampler = textureSamplers[i];
    if (kUnusedUniform != sampler.location) {
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
                                                 const GeometryProcessor& geometryProcessor,
                                                 const Pipeline& pipeline) {
  // we set the textures, and uniforms for installed processors in a generic way.

  // We must bind to texture units in the same order in which we set the uniforms in
  // GLProgramDataManager. That is, we bind textures for processors in this order:
  // geometryProcessor, fragmentProcessors, XferProcessor.
  GLProgramDataManager programDataManager(context, &uniformLocations);
  setRenderTargetState(programDataManager, renderTarget);
  FragmentProcessor::CoordTransformIter coordTransformIter(pipeline);
  glGeometryProcessor->setData(programDataManager, geometryProcessor, &coordTransformIter);
  int nextTexSamplerIdx = 0;
  setFragmentData(programDataManager, pipeline, &nextTexSamplerIdx);

  auto offset = Point::Zero();
  const auto* dstTexture = pipeline.getDstTexture(&offset);
  auto glContext = GLContext::Unwrap(context);
  if (dstTexture) {
    glXferProcessor->setData(programDataManager, *pipeline.getXferProcessor(), dstTexture, offset);
    glContext->bindTexture(nextTexSamplerIdx++, dstTexture->getSampler());
  }
}

void GLProgram::setFragmentData(const GLProgramDataManager& programDataManager,
                                const Pipeline& pipeline, int* nextTexSamplerIdx) {
  FragmentProcessor::Iter iter(pipeline);
  GLFragmentProcessor::Iter glslIter(glFragmentProcessors);
  const FragmentProcessor* fp = iter.next();
  GLFragmentProcessor* glslFP = glslIter.next();
  auto glContext = GLContext::Unwrap(context);
  while (fp && glslFP) {
    glslFP->setData(programDataManager, *fp);
    for (size_t i = 0; i < fp->numTextureSamplers(); ++i) {
      glContext->bindTexture((*nextTexSamplerIdx)++, fp->textureSampler(i), fp->samplerState(i));
    }
    fp = iter.next();
    glslFP = glslIter.next();
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

void GLProgram::setRenderTargetState(const GLProgramDataManager& programDataManager,
                                     const GLRenderTarget* renderTarget) {
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
  programDataManager.set4f(builtinUniformHandles.rtAdjustUniform, v[0], v[1], v[2], v[3]);
}
}  // namespace tgfx
