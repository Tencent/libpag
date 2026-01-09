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

#include "BitmapDrawable.h"
#include "tgfx/gpu/opengl/GLDevice.h"

namespace pag {
std::shared_ptr<BitmapDrawable> BitmapDrawable::Make(int width, int height, void* sharedContext) {
  std::shared_ptr<tgfx::GLDevice> device = nullptr;
  if (sharedContext != nullptr) {
    device = tgfx::GLDevice::Make(sharedContext);
  }
  if (device == nullptr) {
    device = tgfx::GLDevice::MakeWithFallback();
  }
  if (device == nullptr || width <= 0 || height <= 0) {
    return nullptr;
  }
  return std::shared_ptr<BitmapDrawable>(new BitmapDrawable(width, height, std::move(device)));
}

BitmapDrawable::BitmapDrawable(int width, int height, std::shared_ptr<tgfx::Device> device)
    : _width(width), _height(height), device(std::move(device)) {
}

void BitmapDrawable::setBitmap(std::shared_ptr<BitmapBuffer> buffer) {
  pixelCopied = false;
  if (bitmap == buffer) {
    return;
  }
  bitmap = buffer;
  freeSurface();
}

void BitmapDrawable::present(tgfx::Context* context) {
  if (bitmap == nullptr) {
    return;
  }
  auto hardwareBuffer = bitmap->getHardwareBuffer();
  if (hardwareBuffer != nullptr) {
    context->flushAndSubmit(true);
    pixelCopied = true;
    return;
  }
  if (offscreenSurface != nullptr) {
    auto pixels = bitmap->lockPixels();
    if (pixels == nullptr) {
      return;
    }
    pixelCopied = offscreenSurface->readPixels(bitmap->info(), pixels);
    bitmap->unlockPixels();
  }
}

std::shared_ptr<tgfx::Surface> BitmapDrawable::onCreateSurface(tgfx::Context* context) {
  if (bitmap == nullptr) {
    return nullptr;
  }
  auto hardwareBuffer = bitmap->getHardwareBuffer();
  if (hardwareBuffer != nullptr) {
    return tgfx::Surface::MakeFrom(context, hardwareBuffer);
  }
  if (offscreenSurface == nullptr) {
    hardwareBuffer = tgfx::HardwareBufferAllocate(_width, _height);
    offscreenSurface = tgfx::Surface::MakeFrom(context, hardwareBuffer);
    tgfx::HardwareBufferRelease(hardwareBuffer);
    if (offscreenSurface == nullptr) {
      auto colorType = bitmap->info().colorType() == tgfx::ColorType::BGRA_8888
                           ? tgfx::ColorType::BGRA_8888
                           : tgfx::ColorType::RGBA_8888;
      offscreenSurface = tgfx::Surface::Make(context, _width, _height, colorType);
    }
  }
  return offscreenSurface;
}
}  // namespace pag
