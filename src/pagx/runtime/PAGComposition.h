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
#include <unordered_map>
#include <vector>
#include "pagx/nodes/Timeline.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/layers/Layer.h"

namespace pagx {

class Animation;
class Composition;
class Layer;
class PAGFile;
class PAGTimeline;

/**
 * PAGComposition is the runtime instance backing a single Layer.composition slot. Each slot owns
 * its own per-slot layer subtree and timeline list, so multiple Layers referencing the same
 * Composition keep independent state. Children Compositions are constructed recursively.
 *
 * Internal to the runtime; no public API surface exposes this type.
 */
class PAGComposition {
 public:
  /**
   * Builds a runtime slot for ownerLayer.composition. Returns nullptr if ownerLayer is null,
   * has no composition, or the document is not laid out.
   * @param ownerLayer the slot Layer in the source document.
   * @param parentFile the owning PAGFile, used to register spawned PAGTimeline instances. Must not
   *                   be null.
   */
  static std::unique_ptr<PAGComposition> Make(const Layer* ownerLayer, PAGFile* parentFile);

  ~PAGComposition();

  /**
   * The tgfx layer holding this slot's runtime subtree. The owning PAGFile attaches this layer
   * as a child of the slot Layer's tgfx container.
   */
  std::shared_ptr<tgfx::Layer> rootLayer() const {
    return root;
  }

  /**
   * Per-slot binding used by runtime writers when targeting nodes inside this slot.
   */
  RuntimeBinding& mutableBinding() {
    return binding;
  }

  /**
   * The PAGTimeline instances spawned by this slot's Layer.timelines drivers.
   */
  const std::vector<std::shared_ptr<PAGTimeline>>& timelines() const {
    return slotTimelines;
  }

  /**
   * Advances this slot's own spawned timelines, then recursively advances every nested child slot,
   * so the master clock reaches all timelines in the slot subtree.
   */
  void advance(int64_t deltaMicroseconds);

  /**
   * Applies this slot's own spawned timelines, then recursively applies every nested child slot.
   */
  void apply(float mix);

 private:
  PAGComposition(const Layer* ownerLayer, PAGFile* parentFile);

  void buildSubtree();
  void spawnTimelines(const std::vector<std::unique_ptr<Timeline>>& drivers);
  void buildChildSlots();

  const Layer* ownerLayer = nullptr;
  PAGFile* parentFile = nullptr;
  PAGXDocument* document = nullptr;
  std::shared_ptr<tgfx::Layer> root = nullptr;
  RuntimeBinding binding = {};
  std::vector<std::shared_ptr<PAGTimeline>> slotTimelines = {};
  // Recursive child slots: one entry per Layer inside this slot whose composition field is set.
  std::vector<std::unique_ptr<PAGComposition>> childSlots = {};
};

}  // namespace pagx
