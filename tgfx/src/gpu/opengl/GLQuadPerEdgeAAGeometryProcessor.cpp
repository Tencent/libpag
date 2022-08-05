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

  if (geometryProcessor->color.isInitialized()) {
    auto color = varyingHandler->addVarying("Color", ShaderVar::Type::Float4);
    vertBuilder->codeAppendf("%s = %s;", color.vsOut().c_str(),
                             geometryProcessor->color.name().c_str());
    fragBuilder->codeAppendf("%s = %s;", args.outputColor.c_str(), color.fsIn().c_str());
    fragBuilder->codeAppendf("%s.rgb *= %s.a;", args.outputColor.c_str(), args.outputColor.c_str());
  } else {
    fragBuilder->codeAppendf("%s = vec4(1.0);", args.outputColor.c_str());
  }

  // Emit the vertex position to the hardware in the normalized window coordinates it expects.
  args.vertBuilder->emitNormalizedPosition(geometryProcessor->position.name());
}

void GLQuadPerEdgeAAGeometryProcessor::setData(const ProgramDataManager& programDataManager,
                                               const GeometryProcessor&,
                                               FPCoordTransformIter* transformIter) {
  setTransformDataHelper(Matrix::I(), programDataManager, transformIter);
}
}  // namespace tgfx
