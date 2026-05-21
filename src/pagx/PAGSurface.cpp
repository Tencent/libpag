/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include "pagx/PAGSurface.h"
#include "pagx/runtime/PAGSurfaceImpl.h"
#include "tgfx/core/ImageInfo.h"
#include "tgfx/core/Surface.h"
#include "tgfx/gpu/Device.h"
#include "tgfx/gpu/opengl/GLDevice.h"

namespace pagx {

std::shared_ptr<PAGSurface> PAGSurface::MakeOffscreen(int width, int height) {
  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  auto device = tgfx::GLDevice::Make();
  if (device == nullptr) {
    return nullptr;
  }
  auto* context = device->lockContext();
  if (context == nullptr) {
    return nullptr;
  }
  auto tgfxSurface = tgfx::Surface::Make(context, width, height);
  device->unlock();
  if (tgfxSurface == nullptr) {
    return nullptr;
  }
  auto impl = std::make_unique<PAGSurface::Impl>();
  impl->device = std::move(device);
  impl->surface = std::move(tgfxSurface);
  return std::shared_ptr<PAGSurface>(new PAGSurface(std::move(impl), width, height));
}

PAGSurface::PAGSurface(std::unique_ptr<Impl> impl, int width, int height)
    : impl(std::move(impl)), surfaceWidth(width), surfaceHeight(height) {
}

PAGSurface::~PAGSurface() = default;

bool PAGSurface::readPixels(void* dstPixels, size_t dstRowBytes) {
  if (impl == nullptr || impl->surface == nullptr || dstPixels == nullptr) {
    return false;
  }
  auto info = tgfx::ImageInfo::Make(surfaceWidth, surfaceHeight, tgfx::ColorType::RGBA_8888,
                                    tgfx::AlphaType::Premultiplied, dstRowBytes);
  if (info.isEmpty()) {
    return false;
  }
  auto* context = impl->device->lockContext();
  if (context == nullptr) {
    return false;
  }
  bool ok = impl->surface->readPixels(info, dstPixels);
  impl->device->unlock();
  return ok;
}

}  // namespace pagx
