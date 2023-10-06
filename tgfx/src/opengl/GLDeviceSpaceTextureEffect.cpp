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
std::unique_ptr<DeviceSpaceTextureEffect> DeviceSpaceTextureEffect::Make(
    std::shared_ptr<Texture> texture, ImageOrigin deviceOrigin) {
  if (texture == nullptr) {
    return nullptr;
  }
  return std::unique_ptr<DeviceSpaceTextureEffect>(
      new GLDeviceSpaceTextureEffect(std::move(texture), deviceOrigin));
}

GLDeviceSpaceTextureEffect::GLDeviceSpaceTextureEffect(std::shared_ptr<Texture> texture,
                                                       ImageOrigin deviceOrigin)
    : DeviceSpaceTextureEffect(std::move(texture), deviceOrigin) {
}

void GLDeviceSpaceTextureEffect::emitCode(EmitArgs& args) const {
  auto* fragBuilder = args.fragBuilder;
  auto* uniformHandler = args.uniformHandler;
  auto deviceCoordMatrixName = uniformHandler->addUniform(
      ShaderFlags::Fragment, ShaderVar::Type::Float3x3, "DeviceCoordMatrix");
  auto scaleName =
      uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float2, "CoordScale");
  fragBuilder->codeAppendf("vec3 deviceCoord = %s * vec3(gl_FragCoord.xy * %s, 1.0);",
                           deviceCoordMatrixName.c_str(), scaleName.c_str());
  std::string coordName = "deviceCoord.xy";
  fragBuilder->codeAppendf("%s = ", args.outputColor.c_str());
  fragBuilder->appendTextureLookup((*args.textureSamplers)[0], coordName);
  fragBuilder->codeAppend(";");
}

void GLDeviceSpaceTextureEffect::onSetData(UniformBuffer* uniformBuffer) const {
  float scales[] = {1.f / static_cast<float>(texture->width()),
                    1.f / static_cast<float>(texture->height())};
  uniformBuffer->setData("CoordScale", scales);
  uniformBuffer->setMatrix("DeviceCoordMatrix", deviceCoordMatrix);
}
}  // namespace tgfx
