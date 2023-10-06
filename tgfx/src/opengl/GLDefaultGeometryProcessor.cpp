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
#include "gpu/DefaultGeometryProcessor.h"

namespace tgfx {
void GLDefaultGeometryProcessor::emitCode(EmitArgs& args) {
  const auto* geometryProcessor = static_cast<const DefaultGeometryProcessor*>(args.gp);
  auto* vertBuilder = args.vertBuilder;
  auto* fragBuilder = args.fragBuilder;
  auto* varyingHandler = args.varyingHandler;
  auto* uniformHandler = args.uniformHandler;

  varyingHandler->emitAttributes(*geometryProcessor);

  auto matrixName =
      args.uniformHandler->addUniform(ShaderFlags::Vertex, ShaderVar::Type::Float3x3, "Matrix");
  std::string position = "position";
  vertBuilder->codeAppendf("vec2 %s = (%s * vec3(%s, 1.0)).xy;", position.c_str(),
                           matrixName.c_str(), geometryProcessor->position.name().c_str());

  emitTransforms(vertBuilder, varyingHandler, uniformHandler,
                 geometryProcessor->position.asShaderVar(), args.fpCoordTransformHandler);

  auto coverage = varyingHandler->addVarying("Coverage", ShaderVar::Type::Float);
  vertBuilder->codeAppendf("%s = %s;", coverage.vsOut().c_str(),
                           geometryProcessor->coverage.name().c_str());
  fragBuilder->codeAppendf("%s = vec4(%s);", args.outputCoverage.c_str(), coverage.fsIn().c_str());

  auto colorName =
      args.uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float4, "Color");
  fragBuilder->codeAppendf("%s = %s;", args.outputColor.c_str(), colorName.c_str());

  // Emit the vertex position to the hardware in the normalized window coordinates it expects.
  args.vertBuilder->emitNormalizedPosition(position);
}

void GLDefaultGeometryProcessor::setData(UniformBuffer* uniformBuffer,
                                         const GeometryProcessor& geometryProcessor,
                                         FPCoordTransformIter* transformIter) {
  const auto& gp = static_cast<const DefaultGeometryProcessor&>(geometryProcessor);
  setTransformDataHelper(gp.localMatrix, uniformBuffer, transformIter);
  uniformBuffer->setData("Color", gp.color.array());
  uniformBuffer->setMatrix("Matrix", gp.viewMatrix);
}
}  // namespace tgfx
