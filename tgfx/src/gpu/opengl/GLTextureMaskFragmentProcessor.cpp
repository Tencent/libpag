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

#include "GLTextureMaskFragmentProcessor.h"
#include "gpu/TextureMaskFragmentProcessor.h"

namespace tgfx {
void GLTextureMaskFragmentProcessor::emitCode(EmitArgs& args) {
  const auto* textureFP = static_cast<const TextureMaskFragmentProcessor*>(args.fragmentProcessor);
  auto* fragBuilder = args.fragBuilder;
  auto* uniformHandler = args.uniformHandler;
  std::string coordName;
  if (textureFP->useLocalCoord) {
    coordName = (*args.transformedCoords)[0].name();
  } else {
    std::string deviceCoordMatrixName;
    deviceCoordMatrixUniform =
        uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float3x3,
                                   "DeviceCoordMatrix", &deviceCoordMatrixName);
    coordName = "deviceCoord";
    fragBuilder->codeAppendf("vec2 %s = (%s * vec3(gl_FragCoord.xy, 1.0)).xy;", coordName.c_str(),
                             deviceCoordMatrixName.c_str());
    // Clamp to border and the border's color is vec4(0.0).
    fragBuilder->codeAppendf("if (0.0 <= %s.x && %s.x <= 1.0 && 0.0 <= %s.y && %s.y <= 1.0) {",
                             coordName.c_str(), coordName.c_str(), coordName.c_str(),
                             coordName.c_str());
  }
  fragBuilder->codeAppendf("%s = ", args.outputColor.c_str());
  fragBuilder->appendTextureLookup((*args.textureSamplers)[0], coordName);
  fragBuilder->codeAppendf(".aaaa * %s", args.inputColor.c_str());
  fragBuilder->codeAppend(";");
  if (!textureFP->useLocalCoord) {
    fragBuilder->codeAppendf("} else {");
    fragBuilder->codeAppendf("%s = vec4(0.0);", args.outputColor.c_str());
    fragBuilder->codeAppendf("}");
  }
  if (textureFP->inverted) {
    fragBuilder->codeAppendf("%s = vec4(1.0) - %s;", args.outputColor.c_str(),
                             args.outputColor.c_str());
  }
}

void GLTextureMaskFragmentProcessor::onSetData(const ProgramDataManager& programDataManager,
                                               const FragmentProcessor& fragmentProcessor) {
  const auto& textureFP = static_cast<const TextureMaskFragmentProcessor&>(fragmentProcessor);
  if (deviceCoordMatrixUniform.isValid()) {
    if (textureFP.deviceCoordMatrix != deviceCoordMatrixPrev) {
      deviceCoordMatrixPrev = textureFP.deviceCoordMatrix;
      programDataManager.setMatrix(deviceCoordMatrixUniform, deviceCoordMatrixPrev);
    }
  }
}
}  // namespace tgfx
