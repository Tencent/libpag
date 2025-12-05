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

#include "pag_types.h"

PAG_C_PLUS_PLUS_BEGIN_GUARD

/**
 * Creates a new pag_surface for off-screen rendering with the specified size.
 * Release it by pag_release.
 */
PAG_EXPORT pag_surface* pag_surface_make_offscreen(int width, int height);

PAG_EXPORT bool pag_surface_read_pixels(pag_surface* surface, pag_color_type colorType,
                                     pag_alpha_type alphaType, void* dstPixels, size_t dstRowBytes);

PAG_C_PLUS_PLUS_END_GUARD
