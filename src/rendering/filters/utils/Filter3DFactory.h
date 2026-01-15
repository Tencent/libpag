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

#include "rendering/caches/RenderCache.h"
#include "rendering/renderers/FilterRenderer.h"

namespace pag {

std::shared_ptr<tgfx::Image> Apply3DEffects(std::shared_ptr<tgfx::Image> input,
                                            RenderCache* cache, const FilterList* filterList,
                                            const tgfx::Rect& clipBounds,
                                            const tgfx::Point& sourceScale,
                                            tgfx::Rect* filterBounds, tgfx::Point* offset);

bool Has3DSupport();

/**
 * Get or create a depth texture for 3D layer rendering.
 * All 3D layers in the same composition share the same depth texture.
 * @param cache The render cache
 * @param compositionID Unique ID of the composition
 * @param width Required width of the depth texture
 * @param height Required height of the depth texture
 * @return The depth texture, or nullptr if not supported or creation fails
 */
std::shared_ptr<tgfx::Texture> GetDepthTexture3D(RenderCache* cache, ID compositionID, int width,
                                                 int height);

/**
 * Mark a 3D layer as rendered in the current frame.
 * @param cache The render cache
 * @param compositionID Unique ID of the composition
 * @param layerID ID of the layer
 * @return true if this is the first 3D layer for this composition in current frame
 */
bool MarkLayer3DRendered(RenderCache* cache, ID compositionID, ID layerID);

/**
 * Called at the beginning of each frame to prepare 3D rendering state.
 */
void BeginFrame3D(RenderCache* cache);

/**
 * Called at the end of each frame to clean up expired 3D resources.
 */
void EndFrame3D(RenderCache* cache);

/**
 * Release all 3D resources.
 */
void ReleaseAll3D(RenderCache* cache);

}  // namespace pag
