/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "tgfx/platform/HardwareBuffer.h"

namespace pag {
std::shared_ptr<DoubleBufferedDrawable> DoubleBufferedDrawable::Make(
    int width, int height, const BackendTexture& frontTexture, const BackendTexture& backTexture,
    std::shared_ptr<tgfx::Device> device) {
  if (!frontTexture.isValid() || !backTexture.isValid() ||
      frontTexture.backend() != Backend::OPENGL || backTexture.backend() != Backend::OPENGL ||
      device == nullptr) {
    return nullptr;
  }
  auto window = tgfx::DoubleBufferedWindow::Make(std::move(device), ToTGFX(frontTexture),
                                                 ToTGFX(backTexture));
  if (window == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<DoubleBufferedDrawable>(
      new DoubleBufferedDrawable(width, height, std::move(window)));
}

std::shared_ptr<DoubleBufferedDrawable> DoubleBufferedDrawable::Make(
    int width, int height, HardwareBufferRef frontBuffer, HardwareBufferRef backBuffer,
    std::shared_ptr<tgfx::Device> device) {
  if (frontBuffer == nullptr || backBuffer == nullptr || device == nullptr) {
    return nullptr;
  }
  auto window = tgfx::DoubleBufferedWindow::Make(std::move(device), frontBuffer, backBuffer);
  if (window == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<DoubleBufferedDrawable>(
      new DoubleBufferedDrawable(width, height, std::move(window)));
}

DoubleBufferedDrawable::DoubleBufferedDrawable(std::shared_ptr<tgfx::DoubleBufferedWindow> window)
    : window(std::move(window)) {
}

DoubleBufferedDrawable::DoubleBufferedDrawable(int width, int height,
                                               std::shared_ptr<tgfx::DoubleBufferedWindow> window)
    : _width(width), _height(height), window(std::move(window)) {
}

bool DoubleBufferedDrawable::isFront(const BackendTexture& texture) const {
  return window->isFront(ToTGFX(texture));
}

std::shared_ptr<tgfx::Device> DoubleBufferedDrawable::onCreateDevice() {
  if (_width <= 0 || _height <= 0) {
    return nullptr;
  }
  return window->getDevice();
}

std::shared_ptr<tgfx::Surface> DoubleBufferedDrawable::onCreateSurface(tgfx::Context* context) {
  return window->createSurface(context);
}

void DoubleBufferedDrawable::present(tgfx::Context* context) {
  window->present(context);
  surface = window->getBackSurface();
}
}  // namespace pag
