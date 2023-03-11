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

#include "GLDualBlurFragmentProcessor.h"
#include "gpu/DualBlurFragmentProcessor.h"

namespace tgfx {
void GLDualBlurFragmentProcessor::emitCode(EmitArgs& args) {
  auto* fragBuilder = args.fragBuilder;
  std::string blurOffsetName;
  blurOffsetUniform = args.uniformHandler->addUniform(
      ShaderFlags::Fragment, ShaderVar::Type::Float2, "Blur", &blurOffsetName);
  std::string texelSizeName;
  texelSizeUniform = args.uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float2,
                                                     "TexelSize", &texelSizeName);
  std::string tempColor = "tempColor";
  const auto* fp = static_cast<const DualBlurFragmentProcessor*>(args.fragmentProcessor);
  if (fp->passMode == DualBlurPassMode::Down) {
    fragBuilder->codeAppend("const int size = 5;");
    fragBuilder->codeAppendf("vec2 coords[size];");
    fragBuilder->codeAppend("coords[0] = vec2(0.0, 0.0);");
    fragBuilder->codeAppendf("coords[1] = -%s * %s;", texelSizeName.c_str(),
                             blurOffsetName.c_str());
    fragBuilder->codeAppendf("coords[2] = %s * %s;", texelSizeName.c_str(), blurOffsetName.c_str());
    fragBuilder->codeAppendf("coords[3] = vec2(%s.x, -%s.y) * %s;", texelSizeName.c_str(),
                             texelSizeName.c_str(), blurOffsetName.c_str());
    fragBuilder->codeAppendf("coords[4] = -vec2(%s.x, -%s.y) * %s;", texelSizeName.c_str(),
                             texelSizeName.c_str(), blurOffsetName.c_str());
    fragBuilder->codeAppendf("vec4 sum;");
    fragBuilder->codeAppend("for (int i = 0; i < size; i++) {");
    emitChild(0, &tempColor, args,
              [](std::string_view coord) { return std::string(coord) + " + coords[i]"; });
    fragBuilder->codeAppend("if (i == 0) {");
    fragBuilder->codeAppendf("sum = %s * 4.0;", tempColor.c_str());
    fragBuilder->codeAppend("} else {");
    fragBuilder->codeAppendf("sum += %s;", tempColor.c_str());
    fragBuilder->codeAppend("}");
    fragBuilder->codeAppend("}");
    fragBuilder->codeAppendf("%s = sum / 8.0;", args.outputColor.c_str());
  } else {
    fragBuilder->codeAppend("const int size = 8;");
    fragBuilder->codeAppend("vec2 coords[size];");
    fragBuilder->codeAppendf("coords[0] = vec2(-%s.x * 2.0, 0.0) * %s;", texelSizeName.c_str(),
                             blurOffsetName.c_str());
    fragBuilder->codeAppendf("coords[1] = vec2(-%s.x, %s.y) * %s;", texelSizeName.c_str(),
                             texelSizeName.c_str(), blurOffsetName.c_str());
    fragBuilder->codeAppendf("coords[2] = vec2(0.0, %s.y * 2.0) * %s;", texelSizeName.c_str(),
                             blurOffsetName.c_str());
    fragBuilder->codeAppendf("coords[3] = %s * %s;", texelSizeName.c_str(), blurOffsetName.c_str());
    fragBuilder->codeAppendf("coords[4] = vec2(%s.x * 2.0, 0.0) * %s;", texelSizeName.c_str(),
                             blurOffsetName.c_str());
    fragBuilder->codeAppendf("coords[5] = vec2(%s.x, -%s.y) * %s;", texelSizeName.c_str(),
                             texelSizeName.c_str(), blurOffsetName.c_str());
    fragBuilder->codeAppendf("coords[6] = vec2(0.0, -%s.y * 2.0) * %s;", texelSizeName.c_str(),
                             blurOffsetName.c_str());
    fragBuilder->codeAppendf("coords[7] = vec2(-%s.x, -%s.y) * %s;", texelSizeName.c_str(),
                             texelSizeName.c_str(), blurOffsetName.c_str());
    fragBuilder->codeAppend("vec4 sum = vec4(0.0);");
    fragBuilder->codeAppend("for (int i = 0; i < size; i++) {");
    emitChild(0, &tempColor, args,
              [](std::string_view coord) { return std::string(coord) + " + coords[i]"; });
    fragBuilder->codeAppend("if (mod(float(i), 2.0) == 0.0) {");
    fragBuilder->codeAppendf("sum += %s;", tempColor.c_str());
    fragBuilder->codeAppend("} else {");
    fragBuilder->codeAppendf("sum += %s * 2.0;", tempColor.c_str());
    fragBuilder->codeAppend("}");
    fragBuilder->codeAppend("}");
    fragBuilder->codeAppendf("%s = sum / 12.0;", args.outputColor.c_str());
  }
}

void GLDualBlurFragmentProcessor::onSetData(const ProgramDataManager& programDataManager,
                                            const FragmentProcessor& fragmentProcessor) {
  const auto& fp = static_cast<const DualBlurFragmentProcessor&>(fragmentProcessor);
  if (blurOffsetPrev != fp.blurOffset) {
    blurOffsetPrev = fp.blurOffset;
    programDataManager.set2f(blurOffsetUniform, fp.blurOffset.x, fp.blurOffset.y);
  }
  if (texelSizePrev != fp.texelSize) {
    texelSizePrev = fp.texelSize;
    programDataManager.set2f(texelSizeUniform, fp.texelSize.x, fp.texelSize.y);
  }
}
}  // namespace tgfx
