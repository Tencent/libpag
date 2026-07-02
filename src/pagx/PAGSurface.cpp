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
#include "base/utils/TGFXCast.h"
#include "pagx/runtime/Drawable.h"
#include "pagx/runtime/OffscreenDrawable.h"
#include "pagx/runtime/RenderTargetDrawable.h"
#include "pagx/runtime/TextureDrawable.h"
#include "pagx/tgfx.h"
#include "tgfx/core/ImageInfo.h"
#include "tgfx/gpu/opengl/GLDevice.h"

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

std::shared_ptr<PAGSurface> PAGSurface::MakeFrom(const pag::BackendTexture& texture,
                                                 pag::ImageOrigin origin) {
  auto device = tgfx::GLDevice::Current();
  auto drawable = TextureDrawable::MakeFrom(device, pag::ToTGFX(texture), pag::ToTGFX(origin));
  return MakeFrom(drawable);
}

std::shared_ptr<PAGSurface> PAGSurface::MakeFrom(const pag::BackendRenderTarget& renderTarget,
                                                 pag::ImageOrigin origin) {
  auto device = tgfx::GLDevice::Current();
  auto drawable =
      RenderTargetDrawable::MakeFrom(device, pag::ToTGFX(renderTarget), pag::ToTGFX(origin));
  return MakeFrom(drawable);
}

PAGSurface::PAGSurface(std::shared_ptr<Drawable> drawable) : drawable(std::move(drawable)) {
}

PAGSurface::PAGSurface(std::shared_ptr<tgfx::Surface> surface)
    : drawable(Drawable::MakeFrom(std::move(surface))) {
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
  auto info =
      tgfx::ImageInfo::Make(drawable->width(), drawable->height(), tgfx::ColorType::RGBA_8888,
                            tgfx::AlphaType::Premultiplied, dstRowBytes);
  bool ok = !info.isEmpty() && surface->readPixels(info, dstPixels);
  device->unlock();
  return ok;
}

std::shared_ptr<PAGSurface> MakeFrom(const std::shared_ptr<tgfx::Surface>& surface) {
  if (surface == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<PAGSurface>(new PAGSurface(surface));
}

}  // namespace pagx
