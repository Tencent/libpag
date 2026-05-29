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
#include "pagx/runtime/Drawable.h"
#include "pagx/runtime/OffscreenDrawable.h"
#include "tgfx/core/ImageInfo.h"

namespace pagx {

std::shared_ptr<PAGSurface> PAGSurface::MakeOffscreen(int width, int height) {
  auto drawable = OffscreenDrawable::Make(width, height);
  return MakeFrom(drawable);
}

std::shared_ptr<PAGSurface> PAGSurface::MakeFrom(std::shared_ptr<Drawable> drawable) {
  if (drawable == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<PAGSurface>(new PAGSurface(std::move(drawable)));
}

PAGSurface::PAGSurface(std::shared_ptr<Drawable> drawable) : drawable(std::move(drawable)) {
}

PAGSurface::~PAGSurface() = default;

int PAGSurface::width() const {
  return drawable != nullptr ? drawable->width() : 0;
}

int PAGSurface::height() const {
  return drawable != nullptr ? drawable->height() : 0;
}

bool PAGSurface::readPixels(void* dstPixels, size_t dstRowBytes) {
  if (drawable == nullptr || dstPixels == nullptr) {
    return false;
  }
  auto device = drawable->getDevice();
  if (device == nullptr) {
    return false;
  }
  auto* context = device->lockContext();
  if (context == nullptr) {
    return false;
  }
  auto surface = drawable->getSurface(context);
  if (surface == nullptr) {
    device->unlock();
    return false;
  }
  auto info = tgfx::ImageInfo::Make(drawable->width(), drawable->height(),
                                    tgfx::ColorType::RGBA_8888,
                                    tgfx::AlphaType::Premultiplied, dstRowBytes);
  bool ok = !info.isEmpty() && surface->readPixels(info, dstPixels);
  device->unlock();
  return ok;
}

}  // namespace pagx
