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

#include "pag/c/pag_surface.h"
#include "pag_types_priv.h"

pag_surface* pag_surface_make_offscreen(int width, int height) {
  if (auto surface = pag::PAGSurface::MakeOffscreen(width, height)) {
    return new pag_surface(std::move(surface));
  }
  return nullptr;
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
