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

#include "OffscreenDrawable.h"
#include "tgfx/gpu/opengl/GLDevice.h"

namespace pag {
std::shared_ptr<OffscreenDrawable> OffscreenDrawable::Make(int width, int height) {
  auto device = tgfx::GLDevice::MakeWithFallback();
  if (device == nullptr || width <= 0 || height <= 0) {
    return nullptr;
  }
  return std::shared_ptr<OffscreenDrawable>(
      new OffscreenDrawable(width, height, std::move(device)));
}

OffscreenDrawable::OffscreenDrawable(int width, int height, std::shared_ptr<tgfx::Device> device)
    : _width(width), _height(height), device(std::move(device)) {
}

std::shared_ptr<tgfx::Surface> OffscreenDrawable::onCreateSurface(tgfx::Context* context) {
  auto hardwareBuffer = tgfx::HardwareBufferAllocate(_width, _height);
  auto surface = tgfx::Surface::MakeFrom(context, hardwareBuffer);
  tgfx::HardwareBufferRelease(hardwareBuffer);
  if (surface != nullptr) {
    return surface;
  }
#ifdef __APPLE__
  return tgfx::Surface::Make(context, _width, _height, tgfx::ColorType::BGRA_8888);
#else
  return tgfx::Surface::Make(context, _width, _height);
#endif
}
}  // namespace pag
