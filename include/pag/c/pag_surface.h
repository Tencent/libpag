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

/**
 * Submits pending commands. Backends with synchronous completion support also wait for prior
 * work on the surface's device queue. D3D12 surfaces using the same libpag registry device share
 * that queue; the wait does not cover other host queues. WebGPU only guarantees submission, not
 * GPU completion. The caller must not already hold the same device's context lock.
 * @param surface the surface whose pending commands should be submitted.
 * @return true after submission and successful backend device-health checks. On WebGPU true
 *         does not mean that GPU work has completed. Returns false if surface is null, the device
 *         cannot be locked, fails a health check, or its context has been lost.
 */
PAG_EXPORT bool pag_surface_wait_until_completed(pag_surface* surface);

PAG_C_PLUS_PLUS_END_GUARD
