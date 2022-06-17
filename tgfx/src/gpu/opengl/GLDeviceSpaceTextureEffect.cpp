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

#include "GLDeviceSpaceTextureEffect.h"
#include "gpu/DeviceSpaceTextureEffect.h"

namespace tgfx {
void GLDeviceSpaceTextureEffect::emitCode(EmitArgs& args) {
  auto* fragBuilder = args.fragBuilder;
  auto* uniformHandler = args.uniformHandler;
  std::string deviceCoordMatrixName;
  deviceCoordMatrixUniform =
      uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float3x3,
                                 "DeviceCoordMatrix", &deviceCoordMatrixName);
  std::string scaleName;
  scaleUniform = uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float2,
                                            "CoordScale", &scaleName);
  fragBuilder->codeAppendf("vec3 deviceCoord = %s * vec3(gl_FragCoord.xy * %s, 1.0);",
                           deviceCoordMatrixName.c_str(), scaleName.c_str());
  std::string coordName = "deviceCoord.xy";
  fragBuilder->codeAppendf("%s = ", args.outputColor.c_str());
  fragBuilder->appendTextureLookup((*args.textureSamplers)[0], coordName);
  fragBuilder->codeAppend(";");
}

void GLDeviceSpaceTextureEffect::onSetData(const ProgramDataManager& programDataManager,
                                           const FragmentProcessor& fragmentProcessor) {
  const auto& textureFP = static_cast<const DeviceSpaceTextureEffect&>(fragmentProcessor);
  if (textureFP.texture->width() != widthPrev || textureFP.texture->height() != heightPrev) {
    widthPrev = textureFP.texture->width();
    heightPrev = textureFP.texture->height();
    programDataManager.set2f(scaleUniform, 1.f / static_cast<float>(*widthPrev),
                             1.f / static_cast<float>(*heightPrev));
  }
  if (textureFP.deviceCoordMatrix != deviceCoordMatrixPrev) {
    deviceCoordMatrixPrev = textureFP.deviceCoordMatrix;
    programDataManager.setMatrix(deviceCoordMatrixUniform, *deviceCoordMatrixPrev);
  }
}
}  // namespace tgfx
