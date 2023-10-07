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

#include "GLSweepGradientLayout.h"

namespace tgfx {
std::unique_ptr<SweepGradientLayout> SweepGradientLayout::Make(Matrix matrix, float bias,
                                                               float scale) {
  return std::unique_ptr<SweepGradientLayout>(new GLSweepGradientLayout(matrix, bias, scale));
}

GLSweepGradientLayout::GLSweepGradientLayout(Matrix matrix, float bias, float scale)
    : SweepGradientLayout(matrix, bias, scale) {
}

void GLSweepGradientLayout::emitCode(EmitArgs& args) const {
  auto* fragBuilder = args.fragBuilder;
  auto* uniformHandler = args.uniformHandler;
  auto biasName = uniformHandler->addUniform(ShaderFlags::Fragment, SLType::Float, "Bias");
  auto scaleName = uniformHandler->addUniform(ShaderFlags::Fragment, SLType::Float, "Scale");
  fragBuilder->codeAppendf("float angle = atan(-%s.y, -%s.x);",
                           (*args.transformedCoords)[0].name().c_str(),
                           (*args.transformedCoords)[0].name().c_str());
  fragBuilder->codeAppendf("float t = ((angle * 0.15915494309180001 + 0.5) + %s) * %s;",
                           biasName.c_str(), scaleName.c_str());
  fragBuilder->codeAppendf("%s = vec4(t, 1.0, 0.0, 0.0);", args.outputColor.c_str());
}

void GLSweepGradientLayout::onSetData(UniformBuffer* uniformBuffer) const {
  uniformBuffer->setData("Bias", &bias);
  uniformBuffer->setData("Scale", &scale);
}
}  // namespace tgfx
