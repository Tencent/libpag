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

#include "GLQuadPerEdgeAAGeometryProcessor.h"
#include "gpu/QuadPerEdgeAAGeometryProcessor.h"

namespace tgfx {
void GLQuadPerEdgeAAGeometryProcessor::emitCode(EmitArgs& args) {
  const auto* geometryProcessor = static_cast<const QuadPerEdgeAAGeometryProcessor*>(args.gp);
  auto* vertBuilder = args.vertBuilder;
  auto* fragBuilder = args.fragBuilder;
  auto* varyingHandler = args.varyingHandler;
  auto* uniformHandler = args.uniformHandler;

  varyingHandler->emitAttributes(*geometryProcessor);

  std::string matrixUniformName;
  matrixUniform = uniformHandler->addUniform(ShaderFlags::Vertex, ShaderVar::Type::Float3x3,
                                             "Matrix", &matrixUniformName);
  vertBuilder->codeAppendf("vec3 position = %s * vec3(%s.xy, 1);", matrixUniformName.c_str(),
                           geometryProcessor->position.name().c_str());
  std::string screenSizeUniformName;
  screenSizeUniform = uniformHandler->addUniform(ShaderFlags::Vertex, ShaderVar::Type::Float2,
                                                 "ScreenSize", &screenSizeUniformName);
  vertBuilder->codeAppendf("vec2 clipSpace = (position.xy / %s) * 2.0 - 1.0;",
                           screenSizeUniformName.c_str());
  auto position = ShaderVar("clipSpace", ShaderVar::Type::Float2, ShaderVar::TypeModifier::None);

  emitTransforms(vertBuilder, varyingHandler, uniformHandler,
                 geometryProcessor->localCoord.asShaderVar(), args.fpCoordTransformHandler);

  if (geometryProcessor->aa == AAType::Coverage) {
    auto coverage = varyingHandler->addVarying("coverage", ShaderVar::Type::Float);
    vertBuilder->codeAppendf("%s = %s.z;", coverage.vsOut().c_str(),
                             geometryProcessor->position.name().c_str());
    fragBuilder->codeAppendf("%s = vec4(%s);", args.outputCoverage.c_str(),
                             coverage.fsIn().c_str());
  } else {
    fragBuilder->codeAppendf("%s = vec4(1.0);", args.outputCoverage.c_str());
  }

  fragBuilder->codeAppendf("%s = vec4(1.0);", args.outputColor.c_str());

  // Emit the vertex position to the hardware in the normalized window coordinates it expects.
  args.vertBuilder->emitNormalizedPosition(position.name());
}

void GLQuadPerEdgeAAGeometryProcessor::setData(const ProgramDataManager& programDataManager,
                                               const GeometryProcessor& geometryProcessor,
                                               FPCoordTransformIter* transformIter) {
  const auto& gp = static_cast<const QuadPerEdgeAAGeometryProcessor&>(geometryProcessor);
  setTransformDataHelper(Matrix::I(), programDataManager, transformIter);
  if (!updated || viewMatrixPrev != gp.viewMatrix) {
    viewMatrixPrev = gp.viewMatrix;
    programDataManager.setMatrix(matrixUniform, gp.viewMatrix);
  }
  if (!updated || widthPrev != gp.width || heightPrev != gp.height) {
    widthPrev = gp.width;
    heightPrev = gp.height;
    programDataManager.set2f(screenSizeUniform, static_cast<float>(gp.width),
                             static_cast<float>(gp.height));
  }
  updated = true;
}
}  // namespace tgfx
