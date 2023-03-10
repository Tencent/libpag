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

#include "DeviceSpaceTextureEffect.h"
#include "opengl/GLDeviceSpaceTextureEffect.h"

namespace tgfx {
std::unique_ptr<DeviceSpaceTextureEffect> DeviceSpaceTextureEffect::Make(
    std::shared_ptr<Texture> texture, ImageOrigin deviceOrigin) {
  if (texture == nullptr) {
    return nullptr;
  }
  return std::unique_ptr<DeviceSpaceTextureEffect>(
      new DeviceSpaceTextureEffect(std::move(texture), deviceOrigin));
}

DeviceSpaceTextureEffect::DeviceSpaceTextureEffect(std::shared_ptr<Texture> texture,
                                                   ImageOrigin deviceOrigin)
    : FragmentProcessor(ClassID()), texture(std::move(texture)) {
  setTextureSamplerCnt(1);
  if (deviceOrigin == ImageOrigin::BottomLeft) {
    deviceCoordMatrix.postScale(1, -1);
    deviceCoordMatrix.postTranslate(0, 1);
  }
  auto scale = this->texture->getTextureCoord(static_cast<float>(this->texture->width()),
                                              static_cast<float>(this->texture->height()));
  deviceCoordMatrix.postScale(scale.x, scale.y);
}

bool DeviceSpaceTextureEffect::onIsEqual(const FragmentProcessor& processor) const {
  const auto& that = static_cast<const DeviceSpaceTextureEffect&>(processor);
  return texture == that.texture && deviceCoordMatrix == that.deviceCoordMatrix;
}

std::unique_ptr<GLFragmentProcessor> DeviceSpaceTextureEffect::onCreateGLInstance() const {
  return std::make_unique<GLDeviceSpaceTextureEffect>();
}
}  // namespace tgfx
