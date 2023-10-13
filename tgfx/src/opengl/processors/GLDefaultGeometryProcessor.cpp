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

#include "GLDefaultGeometryProcessor.h"

namespace tgfx {
std::unique_ptr<DefaultGeometryProcessor> DefaultGeometryProcessor::Make(
    Color color, int width, int height, const Matrix& viewMatrix, const Matrix& localMatrix) {
  return std::unique_ptr<DefaultGeometryProcessor>(
      new GLDefaultGeometryProcessor(color, width, height, viewMatrix, localMatrix));
}

GLDefaultGeometryProcessor::GLDefaultGeometryProcessor(Color color, int width, int height,
                                                       const Matrix& viewMatrix,
                                                       const Matrix& localMatrix)
    : DefaultGeometryProcessor(color, width, height, viewMatrix, localMatrix) {
}

void GLDefaultGeometryProcessor::emitCode(EmitArgs& args) const {
  auto* vertBuilder = args.vertBuilder;
  auto* fragBuilder = args.fragBuilder;
  auto* varyingHandler = args.varyingHandler;
  auto* uniformHandler = args.uniformHandler;

  varyingHandler->emitAttributes(*this);

  auto matrixName =
      args.uniformHandler->addUniform(ShaderFlags::Vertex, SLType::Float3x3, "Matrix");
  std::string positionName = "position";
  vertBuilder->codeAppendf("vec2 %s = (%s * vec3(%s, 1.0)).xy;", positionName.c_str(),
                           matrixName.c_str(), position.name().c_str());

  emitTransforms(vertBuilder, varyingHandler, uniformHandler, position.asShaderVar(),
                 args.fpCoordTransformHandler);

  auto coverageVar = varyingHandler->addVarying("Coverage", SLType::Float);
  vertBuilder->codeAppendf("%s = %s;", coverageVar.vsOut().c_str(), coverage.name().c_str());
  fragBuilder->codeAppendf("%s = vec4(%s);", args.outputCoverage.c_str(),
                           coverageVar.fsIn().c_str());

  auto colorName = args.uniformHandler->addUniform(ShaderFlags::Fragment, SLType::Float4, "Color");
  fragBuilder->codeAppendf("%s = %s;", args.outputColor.c_str(), colorName.c_str());

  // Emit the vertex position to the hardware in the normalized window coordinates it expects.
  args.vertBuilder->emitNormalizedPosition(positionName);
}

void GLDefaultGeometryProcessor::setData(UniformBuffer* uniformBuffer,
                                         FPCoordTransformIter* transformIter) const {
  setTransformDataHelper(localMatrix, uniformBuffer, transformIter);
  uniformBuffer->setData("Color", color);
  uniformBuffer->setData("Matrix", viewMatrix);
}
}  // namespace tgfx
