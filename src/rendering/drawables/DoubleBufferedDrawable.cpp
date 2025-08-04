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

#include "DoubleBufferedDrawable.h"
#include "base/utils/TGFXCast.h"
#include "tgfx/gpu/opengl/GLDevice.h"
#include "tgfx/platform/HardwareBuffer.h"

namespace pag {
std::shared_ptr<DoubleBufferedDrawable> DoubleBufferedDrawable::Make(
    int width, int height, bool tryHardware, std::shared_ptr<tgfx::Device> device) {
  if (device == nullptr || width <= 0 || height <= 0) {
    return nullptr;
  }
  return std::shared_ptr<DoubleBufferedDrawable>(
      new DoubleBufferedDrawable(width, height, tryHardware, std::move(device)));
}

DoubleBufferedDrawable::DoubleBufferedDrawable(int width, int height, bool tryHardware,
                                               std::shared_ptr<tgfx::Device> device)
    : _width(width), _height(height), tryHardware(tryHardware), device(std::move(device)) {
}

void DoubleBufferedDrawable::onFreeSurface() {
  frontSurface = nullptr;
}

std::shared_ptr<tgfx::Surface> DoubleBufferedDrawable::getFrontSurface(tgfx::Context* context,
                                                                       bool queryOnly) {
  if (context == nullptr) {
    return nullptr;
  }
  if (!queryOnly && frontSurface == nullptr) {
    surface = onCreateSurface(context);
  }
  return frontSurface;
}

std::shared_ptr<tgfx::Surface> DoubleBufferedDrawable::onCreateSurface(tgfx::Context* context) {
  frontSurface = makeSurface(context);
  if (frontSurface == nullptr) {
    return nullptr;
  }
  auto backSurface = makeSurface(context);
  if (backSurface == nullptr) {
    frontSurface = nullptr;
    return nullptr;
  }
  return backSurface;
}

void DoubleBufferedDrawable::present(tgfx::Context*) {
  if (frontSurface == nullptr) {
    return;
  }
  std::swap(frontSurface, surface);
}

std::shared_ptr<tgfx::Surface> DoubleBufferedDrawable::makeSurface(tgfx::Context* context) const {
  if (tryHardware) {
    auto hardwareBuffer = tgfx::HardwareBufferAllocate(_width, _height);
    auto surface = tgfx::Surface::MakeFrom(context, hardwareBuffer);
    tgfx::HardwareBufferRelease(hardwareBuffer);
    if (surface != nullptr) {
      return surface;
    }
  }
#ifdef __APPLE__
  return tgfx::Surface::Make(context, _width, _height, tgfx::ColorType::BGRA_8888);
#else
  return tgfx::Surface::Make(context, _width, _height);
#endif
}
}  // namespace pag
