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
    std::string scaleName;
    scaleUniform = uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float2,
                                              "CoordScale", &scaleName);
    fragBuilder->codeAppendf("vec3 deviceCoord = %s * vec3(gl_FragCoord.xy * %s, 1.0);",
                             deviceCoordMatrixName.c_str(), scaleName.c_str());
    coordName = "deviceCoord.xy";
  }
  fragBuilder->codeAppendf("%s = ", args.outputColor.c_str());
  fragBuilder->appendTextureLookup((*args.textureSamplers)[0], coordName);
  fragBuilder->codeAppendf(".aaaa * %s", args.inputColor.c_str());
  fragBuilder->codeAppend(";");
  // 荣耀畅玩 6x may occur unexpected results.
  fragBuilder->codeAppendf("%s = clamp(%s, 0.0, 1.0);", args.outputColor.c_str(),
                           args.outputColor.c_str());
  if (textureFP->inverted) {
    fragBuilder->codeAppendf("%s = vec4(1.0) - %s;", args.outputColor.c_str(),
                             args.outputColor.c_str());
  }
}

void GLTextureMaskFragmentProcessor::onSetData(const ProgramDataManager& programDataManager,
                                               const FragmentProcessor& fragmentProcessor) {
  const auto& textureFP = static_cast<const TextureMaskFragmentProcessor&>(fragmentProcessor);
  if (scaleUniform.isValid()) {
    if (textureFP.texture->width() != widthPrev || textureFP.texture->height() != heightPrev) {
      widthPrev = textureFP.texture->width();
      heightPrev = textureFP.texture->height();
      programDataManager.set2f(scaleUniform, 1.f / static_cast<float>(*widthPrev),
                               1.f / static_cast<float>(*heightPrev));
    }
  }
  if (deviceCoordMatrixUniform.isValid()) {
    if (textureFP.deviceCoordMatrix != deviceCoordMatrixPrev) {
      deviceCoordMatrixPrev = textureFP.deviceCoordMatrix;
      programDataManager.setMatrix(deviceCoordMatrixUniform, *deviceCoordMatrixPrev);
    }
  }
}
}  // namespace tgfx
