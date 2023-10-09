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

#include "PAGTypesPriv.h"
#include "pag/c/pag_surface.h"
#include "rendering/drawables/DoubleBufferedDrawable.h"
#include "tgfx/opengl/GLDevice.h"

pag_surface* pag_surface_make_offscreen(int width, int height) {
  if (auto surface = pag::PAGSurface::MakeOffscreen(width, height)) {
    return new pag_surface(std::move(surface));
  }
  return nullptr;
}

pag_surface* pag_surface_make_double_buffered(int width, int height,
                                              pag_backend_texture* frontTexture,
                                              pag_backend_texture* backTexture) {
  auto device = tgfx::GLDevice::Current();
  if (auto drawable = pag::DoubleBufferedDrawable::Make(width, height, frontTexture->p,
                                                        backTexture->p, std::move(device))) {
    if (auto surface = pag::PAGSurface::MakeFrom(drawable)) {
      return new pag_surface(std::move(surface), std::move(drawable));
    }
  }
  return nullptr;
}

pag_surface* pag_surface_make_double_buffered_with_hardware_buffer(int width, int height,
                                                                   void* frontBuffer,
                                                                   void* backBuffer) {
  auto device = tgfx::GLDevice::Make();
  if (auto drawable = pag::DoubleBufferedDrawable::Make(
          width, height, static_cast<pag::HardwareBufferRef>(frontBuffer),
          static_cast<pag::HardwareBufferRef>(backBuffer), std::move(device))) {
    if (auto surface = pag::PAGSurface::MakeFrom(drawable)) {
      return new pag_surface(std::move(surface), std::move(drawable));
    }
  }
  return nullptr;
}

bool pag_surface_is_front_buffer(pag_surface* surface, void* buffer) {
  if (surface == nullptr || buffer == nullptr || surface->drawable == nullptr) {
    return false;
  }
  return surface->drawable->isFront(static_cast<pag::HardwareBufferRef>(buffer));
}

bool pag_surface_is_front_texture(pag_surface* surface, pag_backend_texture* texture) {
  if (surface == nullptr || texture == nullptr || surface->drawable == nullptr) {
    return false;
  }
  return surface->drawable->isFront(texture->p);
}

bool pag_surface_read_pixels(pag_surface* surface, pag_color_type colorType,
                             pag_alpha_type alphaType, void* dstPixels, size_t dstRowBytes) {
  if (surface == nullptr) {
    return false;
  }
  pag::ColorType pagColorType;
  if (!FromCColorType(colorType, &pagColorType)) {
    return false;
  }
  pag::AlphaType pagAlphaType;
  if (!FromCAlphaType(alphaType, &pagAlphaType)) {
    return false;
  }
  return surface->p->readPixels(pagColorType, pagAlphaType, dstPixels, dstRowBytes);
}
