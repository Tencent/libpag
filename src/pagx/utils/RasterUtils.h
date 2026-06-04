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

#pragma once

#include <memory>
#include "tgfx/core/Rect.h"

namespace tgfx {
class Context;
class Data;
class Device;
class Layer;
}  // namespace tgfx

namespace pagx {

class ImagePattern;

/**
 * Manages a lazily-created GPU device and context for off-screen rendering.
 * Reuse a single instance across multiple render calls to avoid repeated GL context creation.
 */
class GPUContext {
 public:
  ~GPUContext();

  GPUContext(const GPUContext&) = delete;
  GPUContext& operator=(const GPUContext&) = delete;
  GPUContext() = default;

  tgfx::Context* lockContext();
  void unlock();

 private:
  std::shared_ptr<tgfx::Device> _device = nullptr;
};

/**
 * Returns the rectangle (in `root`'s coordinate space) that the rasterization helpers below use
 * as both the surface size and the on-slide placement of a baked layer. This is the layer's
 * tight global bounds intersected with the layer's own scrollRect (mapped to `root`) when one is
 * present, so callers and renderers agree on a single, scrollRect-clipped extent. Returns an
 * empty rect when the intersection is empty.
 */
tgfx::Rect ComputeRasterizedLayerBounds(const std::shared_ptr<tgfx::Layer>& root,
                                        const std::shared_ptr<tgfx::Layer>& targetLayer);

/**
 * Generalised version of ComputeRasterizedLayerBounds that resolves the bounds in an arbitrary
 * coordinate space (e.g. the target layer's parent for the SVG `<image>` placement path) instead
 * of the document root. Pass nullptr for `coordinateSpace` to use the layer's own world space.
 * Like the root-based variant, the bounds are intersected with the layer's own scrollRect window
 * (mapped to the same coordinate space) when present, so the rasterized PNG and the `<image>`
 * placement agree on a single scrollRect-clipped extent. Returns an empty rect when the
 * intersection is empty.
 */
tgfx::Rect ComputeRasterizedLayerBoundsInSpace(const std::shared_ptr<tgfx::Layer>& targetLayer,
                                               const tgfx::Layer* coordinateSpace);

/**
 * Rasterizes a tgfx layer (including any masks) to a PNG-encoded Data object. `pixelScale` is the
 * ratio of the desired output pixel density to the layer's logical coordinate space (e.g. 2.0 for
 * 2x density). The encoded PNG has dimensions ceil(logicalSize * pixelScale); callers keep using
 * the logical bounds when placing the PNG so that consumers scale it back up on display.
 * If the layer has its own scrollRect, the output is clipped to and sized by the scrollRect's
 * visible window in `root`'s coordinates (since tgfx's Layer::draw deliberately skips applying
 * the layer's own scrollRect).
 * Returns nullptr if the layer has zero bounds or rasterization fails.
 */
std::shared_ptr<tgfx::Data> RenderMaskedLayer(GPUContext* gpu,
                                              const std::shared_ptr<tgfx::Layer>& root,
                                              const std::shared_ptr<tgfx::Layer>& targetLayer,
                                              float pixelScale);

/**
 * Rasterizes a tgfx layer (including any masks) to a PNG-encoded Data object using a custom
 * coordinate space for both the surface sizing (via the target layer's AABB in that space) and
 * the orientation of the rendered pixels. Pass the target layer's parent as
 * `targetCoordinateSpace` when the PNG is nested inside a group whose transform already matches
 * the parent's coordinate space (e.g. SVG `<image>` emitted inside a parent `<g transform=...>`).
 * Passing nullptr falls back to using `root` as the coordinate space. Like the 4-arg overload,
 * the layer's own scrollRect is auto-applied to both the bounds (so the PNG is sized to the
 * visible window) and the canvas (so pixels outside the window are clipped) — the SVG path can
 * therefore place the resulting `<image>` without an extra `clip-path` wrapper.
 * Returns nullptr if the layer has zero bounds or rasterization fails.
 */
std::shared_ptr<tgfx::Data> RenderMaskedLayer(GPUContext* gpu,
                                              const std::shared_ptr<tgfx::Layer>& root,
                                              const std::shared_ptr<tgfx::Layer>& targetLayer,
                                              const tgfx::Layer* targetCoordinateSpace,
                                              float pixelScale);

/**
 * Rasterizes the full scene rooted at `root`, clipped to the global bounds of `targetLayer`, to
 * a PNG-encoded Data object. Unlike RenderMaskedLayer (which draws only the target against an
 * empty canvas), this renders every layer from `root` downward so that backdrop pixels
 * participate in the composite. Use when the target layer's visual result depends on pixels
 * drawn below it — e.g. a non-Normal `Layer.blendMode` that must blend against the backdrop.
 * `pixelScale` controls the output pixel density (see RenderMaskedLayer).
 * Returns nullptr if the target has zero bounds or rasterization fails.
 */
std::shared_ptr<tgfx::Data> RenderLayerCompositeWithBackdrop(
    GPUContext* gpu, const std::shared_ptr<tgfx::Layer>& root,
    const std::shared_ptr<tgfx::Layer>& targetLayer, float pixelScale);

/**
 * Rasterizes a tiled ImagePattern fill to a PNG-encoded Data object. `width`/`height` describe
 * the logical tile size and `offsetX`/`offsetY` the pattern matrix offset in the same logical
 * space. `pixelScale` controls the output pixel density: the encoded PNG has dimensions
 * ceil(width * pixelScale) × ceil(height * pixelScale). Returns nullptr if rendering fails.
 */
std::shared_ptr<tgfx::Data> RenderTiledPattern(GPUContext* gpu, const ImagePattern* pattern,
                                               int width, int height, float offsetX, float offsetY,
                                               float pixelScale);

}  // namespace pagx
