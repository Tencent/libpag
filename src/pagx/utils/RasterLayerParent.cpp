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

#include "pagx/utils/RasterLayerParent.h"
#include "tgfx/layers/Layer.h"

namespace pagx {

namespace {

// Returns the layer that uses `mask` as its mask. TGFX stores the relation only on the mask side
// (Layer::maskOwner), so the owner has to be located from the tree. Owners inside the mask's own
// subtree are not reported: they reference the target as a mask without making the target itself a
// mask child, so the target still has to be drawn with that relationship intact.
std::shared_ptr<tgfx::Layer> FindMaskOwner(const std::shared_ptr<tgfx::Layer>& layer,
                                           const tgfx::Layer* mask) {
  if (layer == nullptr || mask == nullptr || layer.get() == mask) {
    return nullptr;
  }
  if (layer->mask().get() == mask) {
    return layer;
  }
  for (const auto& child : layer->children()) {
    auto owner = FindMaskOwner(child, mask);
    if (owner != nullptr) {
      return owner;
    }
  }
  return nullptr;
}

}  // namespace

RasterLayerParent::~RasterLayerParent() {
  reset();
}

bool RasterLayerParent::prepare(const std::shared_ptr<tgfx::Layer>& root,
                                const std::shared_ptr<tgfx::Layer>& layer,
                                bool releaseExternalMask) {
  reset();
  if (layer == nullptr) {
    return false;
  }
  target = layer;
  parent = layer->parent();
  if (releaseExternalMask && root != nullptr) {
    maskOwner = FindMaskOwner(root, layer.get());
  }
  wrapper = tgfx::Layer::Make();
  if (wrapper == nullptr) {
    reset();
    return false;
  }
  if (parent != nullptr) {
    if (!parent->replaceChild(target, wrapper)) {
      reset();
      return false;
    }
  }
  if (!wrapper->addChild(target)) {
    // replaceChild() may already have moved the target out of its original parent. reset()
    // restores the original tree using the parent and wrapper still held here, so the rollback
    // data must stay intact until then.
    reset();
    return false;
  }
  if (maskOwner != nullptr) {
    maskOwner->setMask(nullptr);
    maskReleased = true;
  }
  return true;
}

bool RasterLayerParent::isValid() const {
  if (wrapper == nullptr || target == nullptr) {
    return false;
  }
  if (wrapper->parent() != parent) {
    return false;
  }
  if (parent == nullptr) {
    return true;
  }
  // target must still occupy the wrapper's only child slot in the original position.
  const auto& children = parent->children();
  for (const auto& child : children) {
    if (child.get() == wrapper.get()) {
      return child->children().size() == 1 && child->children().front().get() == target.get();
    }
  }
  return false;
}

void RasterLayerParent::reset() {
  // Only restore when the wrapper actually took the target's slot. A prepare() that failed before
  // replaceChild() left the tree untouched, and wrapper->parent() would not match in that case.
  if (wrapper != nullptr && wrapper->parent() == parent) {
    if (parent != nullptr) {
      parent->replaceChild(wrapper, target);
    } else if (target != nullptr) {
      target->removeFromParent();
    }
  }
  if (maskReleased && maskOwner != nullptr) {
    maskOwner->setMask(target);
  }
  target = nullptr;
  parent = nullptr;
  maskOwner = nullptr;
  wrapper = nullptr;
  maskReleased = false;
}

}  // namespace pagx
