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
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

// This translation unit is intentionally compiled as C. It keeps the public C API headers valid
// for real C consumers: no C++ includes, no default member initializers, and every D3D12 entry
// point referenced here must resolve at link time.

#include "pag/c/ext/egl/pag_egl_globals.h"
#include "pag/c/pag.h"

#ifdef PAG_C_API_LINK_CHECK
pag_backend_texture* (*pag_c_api_create_d3d12_texture)(pag_d3d12_texture_info, int, int) =
    pag_backend_texture_create_from_d3d12_texture_info;

bool (*pag_c_api_get_d3d12_texture_info)(pag_backend_texture*, pag_d3d12_texture_info*) =
    pag_backend_texture_get_d3d12_texture_info;

bool (*pag_c_api_wait_until_completed)(pag_surface*) = pag_surface_wait_until_completed;
#endif

void pag_c_api_initialize_descriptors(void) {
  pag_gl_texture_info gl = {0};
  pag_vk_image_info vk = {0};
  pag_mtl_texture_info mtl = {0};
  pag_d3d12_texture_info d3d12 = {0};
  egl_globals egl = {0};
  gl.target = 0x0DE1u;
  gl.format = 0x8058u;
  (void)gl;
  (void)vk;
  (void)mtl;
  (void)d3d12;
  (void)egl;
}
