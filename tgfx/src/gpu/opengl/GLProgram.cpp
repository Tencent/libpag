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
GLProgram::GLProgram(Context* context, unsigned programID, const std::vector<Uniform>& uniforms,
                     std::unique_ptr<GLGeometryProcessor> geometryProcessor,
                     std::unique_ptr<GLXferProcessor> xferProcessor,
                     std::vector<std::unique_ptr<GLFragmentProcessor>> fragmentProcessors,
                     std::vector<Attribute> attributes, int vertexStride)
    : Program(context),
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

void GLProgram::updateUniformsAndTextureBindings(const GeometryProcessor& geometryProcessor,
                                                 const Pipeline& pipeline) {
  // we set the textures, and uniforms for installed processors in a generic way.

  // We must bind to texture units in the same order in which we set the uniforms in
  // GLProgramDataManager. That is, we bind textures for processors in this order:
  // geometryProcessor, fragmentProcessors, XferProcessor.
  GLProgramDataManager programDataManager(context, &uniformLocations);
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
      glContext->bindTexture((*nextTexSamplerIdx)++, fp->textureSampler(i));
    }
    fp = iter.next();
    glslFP = glslIter.next();
  }
}
}  // namespace tgfx
