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

#pragma once

#include "pag/c/pag_types.h"

PAG_C_PLUS_PLUS_BEGIN_GUARD

/**
 * Create a new pag_surface for double-buffered off-screen rendering.
 * Release it by pag_release.
 */
PAG_EXPORT pag_surface* pag_surface_make_offscreen_double_buffered(int width, int height,
                                                                bool tryHardware,
                                                                void* sharedContext);

/**
 * Returns the front texture of the surface.
 */
PAG_EXPORT pag_backend_texture* pag_surface_get_front_texture(pag_surface* surface);

/**
 * Returns the back texture of the surface.
 */
PAG_EXPORT pag_backend_texture* pag_surface_get_back_texture(pag_surface* surface);

/**
 * Returns the front hardware buffer of the surface.
 */
PAG_EXPORT void* pag_surface_get_front_hardware_buffer(pag_surface* surface);

/**
 * Returns the back hardware buffer of the surface.
 */
PAG_EXPORT void* pag_surface_get_back_hardware_buffer(pag_surface* surface);

PAG_C_PLUS_PLUS_END_GUARD
