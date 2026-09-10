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
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

// Compile-time sentinel for the pag::Devices glue layer. Each supported backend has its own
// implementation translation unit (Devices_GL.cpp, Devices_Metal.mm, and — in future PRs —
// Devices_Vulkan.cpp / Devices_D3D12.cpp / Devices_WebGPU.cpp). This file exists solely to make
// "backend selected via PAG_USE_* but libpag glue not implemented yet" a compile-time error,
// instead of a late link-time undefined-symbol failure.
//
// When adding a new backend, add both a Devices_<Backend>.{cpp,mm} implementation file and
// remove the corresponding #elif branch below in the same PR.

#if defined(TGFX_USE_OPENGL) || defined(TGFX_USE_METAL)

// OK — implementation lives in Devices_GL.cpp / Devices_Metal.mm.

#elif defined(TGFX_USE_VULKAN)

#error \
    "libpag Vulkan backend is not yet implemented. Add src/rendering/gpu/Devices_Vulkan.cpp " \
    "(see docs/gpu-backend-decoupling.md §3.3 and §6 Step 5)."

#elif defined(TGFX_USE_D3D12)

#error \
    "libpag D3D12 backend is not yet implemented. Add src/rendering/gpu/Devices_D3D12.cpp " \
    "(see docs/gpu-backend-decoupling.md §3.3 and §6 Step 5)."

#elif defined(TGFX_USE_WEBGPU)

#error \
    "libpag WebGPU backend is not yet implemented. Add src/rendering/gpu/Devices_WebGPU.cpp " \
    "(see docs/gpu-backend-decoupling.md §3.3 and §6 Step 5)."

#else

#error \
    "No supported tgfx GPU backend is enabled for libpag. Set one of PAG_USE_OPENGL / " \
    "PAG_USE_METAL / PAG_USE_VULKAN / PAG_USE_D3D12 / PAG_USE_WEBGPU to ON."

#endif
