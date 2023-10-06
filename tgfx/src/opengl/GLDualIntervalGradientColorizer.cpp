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
#include "gpu/gradients/DualIntervalGradientColorizer.h"

namespace tgfx {
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

void GLDualIntervalGradientColorizer::onSetData(UniformBuffer* uniformBuffer,
                                                const FragmentProcessor& fragmentProcessor) {
  const auto& fp = static_cast<const DualIntervalGradientColorizer&>(fragmentProcessor);
  uniformBuffer->setData("scale01", fp.scale01.array());
  uniformBuffer->setData("bias01", fp.bias01.array());
  uniformBuffer->setData("scale23", fp.scale23.array());
  uniformBuffer->setData("bias23", fp.bias23.array());
  uniformBuffer->setData("threshold", &fp.threshold);
}
}  // namespace tgfx
