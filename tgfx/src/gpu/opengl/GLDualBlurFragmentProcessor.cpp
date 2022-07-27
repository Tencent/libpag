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
  std::string tempColor;
  const auto* fp = static_cast<const DualBlurFragmentProcessor*>(args.fragmentProcessor);
  if (fp->passMode == DualBlurPassMode::Down) {
    std::vector<std::string> coords = {
        "", " - " + texelSizeName + " * " + blurOffsetName,
        " + " + texelSizeName + " * " + blurOffsetName,
        " + vec2(" + texelSizeName + ".x, -" + texelSizeName + ".y) * " + blurOffsetName,
        " - vec2(" + texelSizeName + ".x, -" + texelSizeName + ".y) * " + blurOffsetName};
    for (size_t i = 0; i < coords.size(); ++i) {
      tempColor = "tempColor" + std::to_string(i);
      emitChild(0, &tempColor, args,
                [coords, i](std::string_view coord) { return std::string(coord) + coords[i]; });
      if (i == 0) {
        fragBuilder->codeAppendf("vec4 sum = %s * 4.0;", tempColor.c_str());
      } else {
        fragBuilder->codeAppendf("sum += %s;", tempColor.c_str());
      }
    }
    fragBuilder->codeAppendf("%s = sum / 8.0;", args.outputColor.c_str());
  } else {
    std::vector<std::string> coords = {
        " + vec2(-" + texelSizeName + ".x * 2.0, 0.0) * " + blurOffsetName,
        " + vec2(-" + texelSizeName + ".x, " + texelSizeName + ".y) * " + blurOffsetName,
        " + vec2(0.0, " + texelSizeName + ".y * 2.0) * " + blurOffsetName,
        " + " + texelSizeName + " * " + blurOffsetName,
        " + vec2(" + texelSizeName + ".x * 2.0, 0.0) * " + blurOffsetName,
        " + vec2(" + texelSizeName + ".x, -" + texelSizeName + ".y) * " + blurOffsetName,
        " + vec2(0.0, -" + texelSizeName + ".y * 2.0) * " + blurOffsetName,
        " + vec2(-" + texelSizeName + ".x, -" + texelSizeName + ".y) * " + blurOffsetName};
    for (size_t i = 0; i < coords.size(); ++i) {
      tempColor = "tempColor" + std::to_string(i);
      emitChild(0, &tempColor, args,
                [coords, i](std::string_view coord) { return std::string(coord) + coords[i]; });
      if (i == 0) {
        fragBuilder->codeAppendf("vec4 sum = %s;", tempColor.c_str());
      } else if (i % 2 == 1) {
        fragBuilder->codeAppendf("sum += %s * 2.0;", tempColor.c_str());
      } else {
        fragBuilder->codeAppendf("sum += %s;", tempColor.c_str());
      }
    }
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
