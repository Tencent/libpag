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
 * Creates a pag_image object from an array of pixel data, return null if it's not valid pixels.
 * Release it by pag_release.
 */
PAG_EXPORT pag_image* pag_image_from_pixels(const void* pixels, int width, int height, size_t rowBytes,
                                         pag_color_type colorType, pag_alpha_type alphaType);
/**
 * Creates a pag_image object from a hardware buffer. Release it by pag_release.
 */
PAG_EXPORT pag_image* pag_image_from_hardware_buffer(void* buffer);

/**
 * Creates a pag_image object from the specified backend texture, return null if the texture is
 * invalid. Release it by pag_release.
 */
PAG_EXPORT pag_image* pag_image_from_backend_texture(pag_backend_texture* texture);

PAG_EXPORT void pag_image_set_scale_mode(pag_image* image, pag_scale_mode mode);

PAG_C_PLUS_PLUS_END_GUARD
