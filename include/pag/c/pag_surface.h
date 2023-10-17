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

#pragma once

#include "pag_types.h"

PAG_C_PLUS_PLUS_BEGIN_GUARD

/**
 * Creates a new pag_surface for off-screen rendering with the specified size.
 * Release it by pag_release.
 */
PAG_API pag_surface* pag_surface_make_offscreen(int width, int height);

/**
 * Creates a new pag_surface for off-screen rendering with the specified textures.
 * Release it by pag_release.
 */
PAG_API pag_surface* pag_surface_make_double_buffered(int width, int height,
                                                      pag_backend_texture* frontTexture,
                                                      pag_backend_texture* backTexture);

/**
 * Creates a new pag_surface for off-screen rendering with the specified hardware buffers.
 * Release it by pag_release.
 */
PAG_API pag_surface* pag_surface_make_double_buffered_with_hardware_buffer(int width, int height,
                                                                           void* frontBuffer,
                                                                           void* backBuffer);

PAG_API bool pag_surface_is_front_buffer(pag_surface* surface, void* buffer);

PAG_API bool pag_surface_is_front_texture(pag_surface* surface, pag_backend_texture* texture);

PAG_API bool pag_surface_read_pixels(pag_surface* surface, pag_color_type colorType,
                                     pag_alpha_type alphaType, void* dstPixels, size_t dstRowBytes);

PAG_C_PLUS_PLUS_END_GUARD
