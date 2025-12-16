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
 * Creates a new pag_backend_texture with texture info, width and height.
 * Release it by pag_release.
 */
PAG_EXPORT pag_backend_texture* pag_backend_texture_create_from_gl_texture_info(
    pag_gl_texture_info textureInfo, int width, int height);

PAG_EXPORT bool pag_backend_texture_get_gl_texture_info(pag_backend_texture* texture,
                                                     pag_gl_texture_info* textureInfo);

PAG_EXPORT bool pag_backend_texture_get_vk_image_info(pag_backend_texture* texture,
                                                   pag_vk_image_info* imageInfo);

PAG_EXPORT bool pag_backend_texture_get_mtl_texture_info(pag_backend_texture* texture,
                                                      pag_mtl_texture_info* mtl_texture_info);

PAG_C_PLUS_PLUS_END_GUARD
