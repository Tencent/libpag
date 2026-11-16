/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "tgfx/core/Image.h"
#include "tgfx/core/Point.h"
#include "tgfx/core/Rect.h"
#include "tgfx/gpu/GPU.h"

namespace pag {
class Layer;
class RenderCache;
struct FilterList;

std::shared_ptr<tgfx::Image> Apply3DEffects(std::shared_ptr<tgfx::Image> input, RenderCache* cache,
                                            const FilterList* filterList,
                                            const tgfx::Rect& clipBounds,
                                            const tgfx::Point& sourceScale,
                                            tgfx::Rect* filterBounds, tgfx::Point* offset);

/** Returns true if the GPU supports the formats and resource limits required by 3D projection and
 * per-pixel transparent compositing. */
bool Has3DSupport(const tgfx::GPU* gpu);

/**
 * Clamps the sampling scale of 3D content to the GPU texture limits and the memory budget, and
 * returns zero for invalid input. memoryBudget is the budget in bytes, usually the cache limit of
 * the Context; textureCount is the number of full-size 4-byte-per-pixel textures held at the
 * target resolution at the same time. When over budget the scale is downsampled by area ratio
 * instead of failing outright.
 */
float Clamp3DScale(const tgfx::GPU* gpu, size_t memoryBudget, int textureCount, float width,
                   float height, float scale);

/** Returns true if the layer itself or any parent in its chain has a 3D transform. */
bool Has3DTransform(const Layer* layer);

/**
 * Returns true if the clip space Y axis of the current GPU backend is flipped relative to the
 * top-left content convention, so a vertical flip is required before projection. The backend
 * difference is encapsulated here instead of scattering Y axis conventions across matrix and
 * vertex code.
 */
bool NeedsClipSpaceYFlip();

}  // namespace pag
