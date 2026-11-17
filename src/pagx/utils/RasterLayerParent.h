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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>

namespace tgfx {
class Layer;
}  // namespace tgfx

namespace pagx {

/**
 * Temporarily inserts a content-free identity parent above a layer so that normal child
 * compositing applies the layer's transform, alpha, visibility, mask, scrollRect and blend exactly
 * once. An instance wraps a single target for as long as the prepared state is kept: the scene path
 * keeps it across frames, while the raster and CLI paths keep it for one draw. The original parent,
 * child order and any externally released mask owner are restored by reset() or destruction.
 * Instances are non-copyable and never cache a raw pointer to a tree that may already be gone.
 */
class RasterLayerParent {
 public:
  RasterLayerParent() = default;
  ~RasterLayerParent();

  RasterLayerParent(const RasterLayerParent&) = delete;
  RasterLayerParent& operator=(const RasterLayerParent&) = delete;

  /**
   * Inserts the identity wrapper above `layer`. When `releaseExternalMask` is set, a layer outside
   * `layer`'s own subtree that references it as a mask is temporarily released so the mask skip
   * rule cannot drop it from the output; the relationship is restored on reset(). Raster exporters
   * leave the flag off to keep their existing mask handling.
   * @param root the root of the tree the target belongs to; used only to locate an external mask
   *        owner and may be null.
   * @param layer the layer to wrap.
   * @param releaseExternalMask whether to temporarily release an external mask owner.
   * @return false and leave the tree unchanged when the layer cannot be wrapped; any partial edit
   *         is rolled back before returning.
   */
  bool prepare(const std::shared_ptr<tgfx::Layer>& root, const std::shared_ptr<tgfx::Layer>& layer,
               bool releaseExternalMask);

  /**
   * Returns true while the prepared wrapper still sits where the target used to be and the target
   * is still its child. Returns false before prepare(), after reset() or if the tree changed under
   * the prepared state.
   */
  bool isValid() const;

  /**
   * Returns the identity wrapper, or null when no valid preparation exists.
   */
  const std::shared_ptr<tgfx::Layer>& layer() const {
    return wrapper;
  }

  /**
   * Restores the original parent, child order and mask ownership, then drops all prepared state.
   * Idempotent. Must run before the owning runtime tree is torn down because the original parent is
   * a borrowed pointer into that tree.
   */
  void reset();

 private:
  std::shared_ptr<tgfx::Layer> target = nullptr;
  tgfx::Layer* parent = nullptr;
  std::shared_ptr<tgfx::Layer> maskOwner = nullptr;
  std::shared_ptr<tgfx::Layer> wrapper = nullptr;
  bool maskReleased = false;
};

}  // namespace pagx
