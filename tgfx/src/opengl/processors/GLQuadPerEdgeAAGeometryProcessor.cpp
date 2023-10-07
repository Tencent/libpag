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

namespace tgfx {
std::unique_ptr<QuadPerEdgeAAGeometryProcessor> QuadPerEdgeAAGeometryProcessor::Make(
    int width, int height, AAType aa, bool hasColor) {
  return std::unique_ptr<QuadPerEdgeAAGeometryProcessor>(
      new GLQuadPerEdgeAAGeometryProcessor(width, height, aa, hasColor));
}

GLQuadPerEdgeAAGeometryProcessor::GLQuadPerEdgeAAGeometryProcessor(int width, int height, AAType aa,
                                                                   bool hasColor)
    : QuadPerEdgeAAGeometryProcessor(width, height, aa, hasColor) {
}

void GLQuadPerEdgeAAGeometryProcessor::emitCode(EmitArgs& args) const {
  auto* vertBuilder = args.vertBuilder;
  auto* fragBuilder = args.fragBuilder;
  auto* varyingHandler = args.varyingHandler;
  auto* uniformHandler = args.uniformHandler;

  varyingHandler->emitAttributes(*this);

  emitTransforms(vertBuilder, varyingHandler, uniformHandler, localCoord.asShaderVar(),
                 args.fpCoordTransformHandler);

  if (aa == AAType::Coverage) {
    auto coverage = varyingHandler->addVarying("coverage", SLType::Float);
    vertBuilder->codeAppendf("%s = %s.z;", coverage.vsOut().c_str(), position.name().c_str());
    fragBuilder->codeAppendf("%s = vec4(%s);", args.outputCoverage.c_str(),
                             coverage.fsIn().c_str());
  } else {
    fragBuilder->codeAppendf("%s = vec4(1.0);", args.outputCoverage.c_str());
  }

  if (color.isInitialized()) {
    auto colorVar = varyingHandler->addVarying("Color", SLType::Float4);
    vertBuilder->codeAppendf("%s = %s;", colorVar.vsOut().c_str(), color.name().c_str());
    fragBuilder->codeAppendf("%s = %s;", args.outputColor.c_str(), colorVar.fsIn().c_str());
  } else {
    fragBuilder->codeAppendf("%s = vec4(1.0);", args.outputColor.c_str());
  }

  // Emit the vertex position to the hardware in the normalized window coordinates it expects.
  args.vertBuilder->emitNormalizedPosition(position.name());
}

void GLQuadPerEdgeAAGeometryProcessor::setData(UniformBuffer* uniformBuffer,
                                               FPCoordTransformIter* transformIter) const {
  setTransformDataHelper(Matrix::I(), uniformBuffer, transformIter);
}
}  // namespace tgfx
