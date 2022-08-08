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

#include "GLRGBAAATextureEffect.h"
#include "gpu/RGBAAATextureEffect.h"

namespace tgfx {
void GLRGBAAATextureEffect::emitCode(EmitArgs& args) {
  const auto* textureFP = static_cast<const RGBAAATextureEffect*>(args.fragmentProcessor);
  auto* fragBuilder = args.fragBuilder;
  auto* uniformHandler = args.uniformHandler;

  auto vertexColor = (*args.transformedCoords)[0].name();
  if (args.coordFunc) {
    vertexColor = args.coordFunc(vertexColor);
  }
  fragBuilder->codeAppend("vec4 color = ");
  fragBuilder->appendTextureLookup((*args.textureSamplers)[0], vertexColor);
  fragBuilder->codeAppend(";");
  if (textureFP->layout != nullptr) {
    fragBuilder->codeAppend("color = clamp(color, 0.0, 1.0);");
    std::string alphaStartName;
    alphaStartUniform = uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float2,
                                                   "alphaStart", &alphaStartName);
    std::string alphaVertexColor = "alphaVertexColor";
    fragBuilder->codeAppendf("vec2 %s = %s + %s;", alphaVertexColor.c_str(), vertexColor.c_str(),
                             alphaStartName.c_str());
    fragBuilder->codeAppend("vec4 alpha = ");
    fragBuilder->appendTextureLookup((*args.textureSamplers)[0], alphaVertexColor);
    fragBuilder->codeAppend(";");
    fragBuilder->codeAppend("alpha = clamp(alpha, 0.0, 1.0);");
    fragBuilder->codeAppend("color = vec4(color.rgb * alpha.r, alpha.r);");
  }
  fragBuilder->codeAppendf("%s = color * %s;", args.outputColor.c_str(), args.inputColor.c_str());
}

void GLRGBAAATextureEffect::onSetData(const ProgramDataManager& programDataManager,
                                      const FragmentProcessor& fragmentProcessor) {
  const auto& textureFP = static_cast<const RGBAAATextureEffect&>(fragmentProcessor);
  if (alphaStartUniform.isValid()) {
    auto alphaStart =
        textureFP.texture->getTextureCoord(static_cast<float>(textureFP.layout->alphaStartX),
                                           static_cast<float>(textureFP.layout->alphaStartY));
    if (alphaStartPrev != alphaStart) {
      alphaStartPrev = alphaStart;
      programDataManager.set2f(alphaStartUniform, static_cast<float>(alphaStart.x),
                               static_cast<float>(alphaStart.y));
    }
  }
}
}  // namespace tgfx
