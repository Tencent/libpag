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

/**
 * Creates a new pag_backend_texture from a Metal id<MTLTexture>. Release it by pag_release.
 * The MTLTexture pointer inside textureInfo is stored as an opaque handle; libpag will not
 * release it.
 */
PAG_EXPORT pag_backend_texture* pag_backend_texture_create_from_mtl_texture_info(
    pag_mtl_texture_info textureInfo, int width, int height);

/**
 * Creates a new pag_backend_texture from an external ID3D12Resource. Release it by pag_release.
 * The resource pointer is stored as an opaque handle; libpag will not release it. The resource
 * must be a single-sample, single-plane typed 2D texture in D3D12_RESOURCE_STATE_COMMON. Sampling
 * requires DENY_SHADER_RESOURCE to be unset; rendering requires ALLOW_RENDER_TARGET. Supported
 * formats are R8_UNORM and RGBA8/BGRA8 UNORM or SRGB. The native device must match the surface
 * that samples the texture. Typeless, array, multisampled and multi-plane resources are rejected.
 */
PAG_EXPORT pag_backend_texture* pag_backend_texture_create_from_d3d12_texture_info(
    pag_d3d12_texture_info textureInfo, int width, int height);

PAG_EXPORT bool pag_backend_texture_get_gl_texture_info(pag_backend_texture* texture,
                                                     pag_gl_texture_info* textureInfo);

PAG_EXPORT bool pag_backend_texture_get_vk_image_info(pag_backend_texture* texture,
                                                   pag_vk_image_info* imageInfo);

PAG_EXPORT bool pag_backend_texture_get_mtl_texture_info(pag_backend_texture* texture,
                                                      pag_mtl_texture_info* mtl_texture_info);

PAG_EXPORT bool pag_backend_texture_get_d3d12_texture_info(pag_backend_texture* texture,
                                                        pag_d3d12_texture_info* textureInfo);

PAG_C_PLUS_PLUS_END_GUARD
