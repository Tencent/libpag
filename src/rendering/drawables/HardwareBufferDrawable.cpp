/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "HardwareBufferDrawable.h"
#include "tgfx/gpu/opengl/GLDevice.h"
#include "tgfx/platform/HardwareBuffer.h"

namespace pag {
std::shared_ptr<HardwareBufferDrawable> HardwareBufferDrawable::MakeFrom(
    HardwareBufferRef hardwareBuffer, std::shared_ptr<tgfx::Device> device) {
  auto info = tgfx::HardwareBufferGetInfo(hardwareBuffer);
  if (info.width <= 0 || info.height <= 0) {
    return nullptr;
  }
  if (device == nullptr) {
    device = tgfx::GLDevice::MakeWithFallback();
  }
  if (device == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<HardwareBufferDrawable>(
      new HardwareBufferDrawable(info.width, info.height, hardwareBuffer, std::move(device)));
}

HardwareBufferDrawable::HardwareBufferDrawable(int width, int height,
                                               HardwareBufferRef hardwareBuffer,
                                               std::shared_ptr<tgfx::Device> device)
    : _width(width), _height(height), hardwareBuffer(hardwareBuffer), device(std::move(device)) {
}

std::shared_ptr<tgfx::Surface> HardwareBufferDrawable::onCreateSurface(tgfx::Context* context) {
  return tgfx::Surface::MakeFrom(context, hardwareBuffer);
}
}  // namespace pag
