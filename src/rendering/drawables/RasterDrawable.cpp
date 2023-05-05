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

#include "RasterDrawable.h"
#include "tgfx/opengl/GLDevice.h"

namespace pag {
std::shared_ptr<RasterDrawable> RasterDrawable::Make(int width, int height) {
  auto device = tgfx::GLDevice::MakeFromThreadPool();
  if (device == nullptr || width <= 0 || height <= 0) {
    return nullptr;
  }
  return std::shared_ptr<RasterDrawable>(new RasterDrawable(width, height, std::move(device)));
}

RasterDrawable::RasterDrawable(int width, int height, std::shared_ptr<tgfx::Device> device)
    : OffscreenDrawable(width, height, device) {
}

void RasterDrawable::setHardwareBuffer(tgfx::HardwareBufferRef buffer) {
  pixelCopied = false;
  if (hardwareBuffer == buffer) {
    return;
  }
  hardwareBuffer = buffer;
  info = {};
  pixels = nullptr;
  freeSurface();
}

void RasterDrawable::setPixelBuffer(const tgfx::ImageInfo& newInfo, void* newPixels) {
  pixelCopied = false;
  if (info == newInfo && pixels == newPixels) {
    return;
  }
  info = newInfo;
  pixels = newInfo.isEmpty() ? nullptr : newPixels;
  hardwareBuffer = nullptr;
  freeSurface();
}

void RasterDrawable::present(tgfx::Context*) {
  if (pixels == nullptr || offscreenSurface == nullptr) {
    pixelCopied = true;
    return;
  }
  pixelCopied = offscreenSurface->readPixels(info, pixels);
}

std::shared_ptr<tgfx::Surface> RasterDrawable::onCreateSurface(tgfx::Context* context) {
  if (hardwareBuffer != nullptr) {
    return tgfx::Surface::MakeFrom(context, hardwareBuffer);
  }
  if (pixels == nullptr) {
    return nullptr;
  }
  if (offscreenSurface == nullptr) {
    offscreenSurface = OffscreenDrawable::onCreateSurface(context);
  }
  return offscreenSurface;
}
}  // namespace pag
