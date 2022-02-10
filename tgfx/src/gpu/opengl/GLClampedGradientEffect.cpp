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

#include "GLClampedGradientEffect.h"
#include "gpu/gradients/ClampedGradientEffect.h"

namespace pag {
void GLClampedGradientEffect::emitCode(EmitArgs& args) {
  auto* fragBuilder = args.fragBuilder;
  const auto* fp = static_cast<const ClampedGradientEffect*>(args.fragmentProcessor);
  std::string leftBorderColorName;
  leftBorderColorUniform = args.uniformHandler->addUniform(
      ShaderFlags::Fragment, ShaderVar::Type::Float4, "leftBorderColor", &leftBorderColorName);
  std::string rightBorderColorName;
  rightBorderColorUniform = args.uniformHandler->addUniform(
      ShaderFlags::Fragment, ShaderVar::Type::Float4, "rightBorderColor", &rightBorderColorName);
  std::string _child1 = "_child1";
  emitChild(fp->gradLayoutIndex, &_child1, args);
  fragBuilder->codeAppendf("vec4 t = %s;", _child1.c_str());
  fragBuilder->codeAppend("if (t.y < 0.0) {");
  fragBuilder->codeAppendf("%s = vec4(0.0);", args.outputColor.c_str());
  fragBuilder->codeAppend("} else if (t.x <= 0.0) {");
  fragBuilder->codeAppendf("%s = %s;", args.outputColor.c_str(), leftBorderColorName.c_str());
  fragBuilder->codeAppend("} else if (t.x >= 1.0) {");
  fragBuilder->codeAppendf("%s = %s;", args.outputColor.c_str(), rightBorderColorName.c_str());
  fragBuilder->codeAppend("} else {");
  std::string _input0 = "t";
  std::string _child0 = "_child0";
  emitChild(fp->colorizerIndex, _input0, &_child0, args);
  fragBuilder->codeAppendf("%s = %s;", args.outputColor.c_str(), _child0.c_str());
  fragBuilder->codeAppend("}");
  if (fp->makePremultiply) {
    fragBuilder->codeAppend("{");
    fragBuilder->codeAppendf("%s.rgb *= %s.a;", args.outputColor.c_str(), args.outputColor.c_str());
    fragBuilder->codeAppend("}");
  }
}

void GLClampedGradientEffect::onSetData(const ProgramDataManager& programDataManager,
                                        const FragmentProcessor& fragmentProcessor) {
  const auto& fp = static_cast<const ClampedGradientEffect&>(fragmentProcessor);
  if (leftBorderColorPrev != fp.leftBorderColor) {
    leftBorderColorPrev = fp.leftBorderColor;
    programDataManager.set4fv(leftBorderColorUniform, 1, fp.leftBorderColor.vector());
  }
  if (rightBorderColorPrev != fp.rightBorderColor) {
    rightBorderColorPrev = fp.rightBorderColor;
    programDataManager.set4fv(rightBorderColorUniform, 1, fp.rightBorderColor.vector());
  }
}
}  // namespace pag
