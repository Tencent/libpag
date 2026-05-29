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
   * @param parentFile the owning PAGFile; used to dispatch ChannelRegistry writes and to register
   *                   spawned PAGTimeline instances. Must not be null.
   */
  static std::unique_ptr<PAGComposition> Make(const Layer* ownerLayer, PAGFile* parentFile);

  ~PAGComposition();

  /**
   * The tgfx layer holding this slot's runtime subtree. The owning PAGFile attaches this layer
   * as a child of the slot Layer's tgfx container.
   */
  std::shared_ptr<tgfx::Layer> rootLayer() const {
    return layerTree.root;
  }

  /**
   * Per-slot layer map used by ChannelRegistry writers when targeting nodes inside this slot.
   */
  PAGLayerTree& mutableLayerTree() {
    return layerTree;
  }

  /**
   * The PAGTimeline instances spawned by this slot's Layer.timelines drivers.
   */
  const std::vector<std::shared_ptr<PAGTimeline>>& timelines() const {
    return slotTimelines;
  }

 private:
  PAGComposition(const Layer* ownerLayer, PAGFile* parentFile);

  void buildSubtree();
  void spawnTimelines(const std::vector<std::unique_ptr<Timeline>>& drivers);
  void buildChildSlots();

  const Layer* ownerLayer = nullptr;
  PAGFile* parentFile = nullptr;
  // The document whose nodeMap drives animation lookup and channel target resolution for this
  // slot. Equal to parentFile->document for in-document Compositions; equal to
  // ownerLayer->composition->externalDoc for sealed cross-document wrappers.
  PAGXDocument* effectiveDoc = nullptr;
  PAGLayerTree layerTree = {};
  std::vector<std::shared_ptr<PAGTimeline>> slotTimelines = {};
  // Recursive child slots: one entry per Layer inside this slot whose composition field is set.
  std::vector<std::unique_ptr<PAGComposition>> childSlots = {};
};

}  // namespace pagx
