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

#include "GLDualIntervalGradientColorizer.h"
#include "gpu/DualIntervalGradientColorizer.h"

namespace pag {
void GLDualIntervalGradientColorizer::emitCode(EmitArgs& args) {
  auto* fragBuilder = args.fragBuilder;
  std::string scale01Name;
  scale01Uniform = args.uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float4,
                                                   "scale01", &scale01Name);
  std::string bias01Name;
  bias01Uniform = args.uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float4,
                                                  "bias01", &bias01Name);
  std::string scale23Name;
  scale23Uniform = args.uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float4,
                                                   "scale23", &scale23Name);
  std::string bias23Name;
  bias23Uniform = args.uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float4,
                                                  "bias23", &bias23Name);
  std::string thresholdName;
  thresholdUniform = args.uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float,
                                                     "threshold", &thresholdName);
  fragBuilder->codeAppendf("float t = %s.x;", args.inputColor.c_str());
  fragBuilder->codeAppend("vec4 scale, bias;");
  fragBuilder->codeAppendf("if (t < %s) {", thresholdName.c_str());
  fragBuilder->codeAppendf("scale = %s;", scale01Name.c_str());
  fragBuilder->codeAppendf("bias = %s;", bias01Name.c_str());
  fragBuilder->codeAppend("} else {");
  fragBuilder->codeAppendf("scale = %s;", scale23Name.c_str());
  fragBuilder->codeAppendf("bias = %s;", bias23Name.c_str());
  fragBuilder->codeAppend("}");
  fragBuilder->codeAppendf("%s = vec4(t * scale + bias);", args.outputColor.c_str());
}

void GLDualIntervalGradientColorizer::onSetData(const ProgramDataManager& programDataManager,
                                                const FragmentProcessor& fragmentProcessor) {
  const auto& fp = static_cast<const DualIntervalGradientColorizer&>(fragmentProcessor);
  if (scale01Prev != fp.scale01) {
    scale01Prev = fp.scale01;
    programDataManager.set4fv(scale01Uniform, 1, fp.scale01.vec());
  }
  if (bias01Prev != fp.bias01) {
    bias01Prev = fp.bias01;
    programDataManager.set4fv(bias01Uniform, 1, fp.bias01.vec());
  }
  if (scale23Prev != fp.scale23) {
    scale23Prev = fp.scale23;
    programDataManager.set4fv(scale23Uniform, 1, fp.scale23.vec());
  }
  if (bias23Prev != fp.bias23) {
    bias23Prev = fp.bias23;
    programDataManager.set4fv(bias23Uniform, 1, fp.bias23.vec());
  }
  if (thresholdPrev != fp.threshold) {
    thresholdPrev = fp.threshold;
    programDataManager.set1f(thresholdUniform, fp.threshold);
  }
}
}  // namespace pag
