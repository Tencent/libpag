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

  std::string screenSizeUniformName;
  screenSizeUniform = uniformHandler->addUniform(ShaderFlags::Vertex, ShaderVar::Type::Float2,
                                                 "ScreenSize", &screenSizeUniformName);
  auto clipSpace = ShaderVar("clipSpace", ShaderVar::Type::Float2, ShaderVar::TypeModifier::None);
  vertBuilder->codeAppendf("vec2 %s = (%s / %s) * 2.0 - 1.0;", clipSpace.name().c_str(),
                           geometryProcessor->position.name().c_str(),
                           screenSizeUniformName.c_str());

  emitTransforms(vertBuilder, varyingHandler, uniformHandler,
                 geometryProcessor->position.asShaderVar(), args.fpCoordTransformHandler);

  auto coverage = varyingHandler->addVarying("Coverage", ShaderVar::Type::Float);
  vertBuilder->codeAppendf("%s = %s;", coverage.vsOut().c_str(),
                           geometryProcessor->coverage.name().c_str());
  fragBuilder->codeAppendf("%s = vec4(%s);", args.outputCoverage.c_str(), coverage.fsIn().c_str());
  fragBuilder->codeAppendf("%s = vec4(1.0);", args.outputColor.c_str());

  // Emit the vertex position to the hardware in the normalized window coordinates it expects.
  args.vertBuilder->emitNormalizedPosition(clipSpace.name());
}

void GLDefaultGeometryProcessor::setData(const ProgramDataManager& programDataManager,
                                         const GeometryProcessor& geometryProcessor,
                                         FPCoordTransformIter* transformIter) {
  const auto& gp = static_cast<const DefaultGeometryProcessor&>(geometryProcessor);
  setTransformDataHelper(Matrix::I(), programDataManager, transformIter);
  if (widthPrev != gp.width || heightPrev != gp.height) {
    widthPrev = gp.width;
    heightPrev = gp.height;
    programDataManager.set2f(screenSizeUniform, static_cast<float>(gp.width),
                             static_cast<float>(gp.height));
  }
}
}  // namespace tgfx
