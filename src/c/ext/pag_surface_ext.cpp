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

#include "pag/c/ext/pag_surface_ext.h"
#include "c/pag_types_priv.h"
#include "pag/pag.h"
#include "rendering/drawables/DoubleBufferedDrawable.h"
#include "tgfx/gpu/opengl/GLDevice.h"

pag_surface* pag_surface_make_offscreen_double_buffered(int width, int height, bool tryHardware,
                                                        void* sharedContext) {
  auto device = tgfx::GLDevice::Make(sharedContext);
  if (device == nullptr) {
    return nullptr;
  }
  if (auto surface = pag::PAGSurface::MakeFrom(
          pag::DoubleBufferedDrawable::Make(width, height, tryHardware, device))) {
    return new pag_surface(std::move(surface));
  }
  return nullptr;
}

pag_backend_texture* pag_surface_get_front_texture(pag_surface* surface) {
  if (surface == nullptr) {
    return nullptr;
  }
  auto texture = surface->ext->getFrontTexture();
  if (!texture.isValid()) {
    return nullptr;
  }
  return new pag_backend_texture(texture);
}

pag_backend_texture* pag_surface_get_back_texture(pag_surface* surface) {
  if (surface == nullptr) {
    return nullptr;
  }
  auto texture = surface->ext->getBackTexture();
  if (!texture.isValid()) {
    return nullptr;
  }
  return new pag_backend_texture(texture);
}

void* pag_surface_get_front_hardware_buffer(pag_surface* surface) {
  if (surface == nullptr) {
    return nullptr;
  }
  return surface->ext->getFrontHardwareBuffer();
}

void* pag_surface_get_back_hardware_buffer(pag_surface* surface) {
  if (surface == nullptr) {
    return nullptr;
  }
  return surface->ext->getBackHardwareBuffer();
}

pag_surface* pag_surface_make_from_texture(pag_backend_texture* texture, pag_image_origin origin,
                                           bool forAsyncThread) {
  if (texture == nullptr || !texture->p.isValid()) {
    return nullptr;
  }
  pag::ImageOrigin pagOrigin;
  if (!FromCImageOrigin(origin, &pagOrigin)) {
    return nullptr;
  }
  if (auto surface = pag::PAGSurface::MakeFrom(texture->p, pagOrigin, forAsyncThread)) {
    return new pag_surface(std::move(surface));
  }
  return nullptr;
}
