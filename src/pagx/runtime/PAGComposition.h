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
#include <vector>
#include "pagx/PAGComposition.h"
#include "pagx/nodes/Timeline.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/layers/Layer.h"

namespace pagx {

class PAGFile;
class PAGTimeline;
class PAGXDocument;

/**
 * Internal runtime state for a PAGComposition. Defined in the internal header because it exposes
 * tgfx and RuntimeBinding, which must not appear in the public include/pagx/PAGComposition.h.
 */
struct PAGComposition::Impl {
  const Layer* ownerLayer = nullptr;
  PAGFile* parentFile = nullptr;
  PAGXDocument* document = nullptr;
  std::shared_ptr<tgfx::Layer> root = nullptr;
  RuntimeBinding binding = {};
  std::vector<std::shared_ptr<PAGTimeline>> timelines = {};
  // Recursive child compositions: one entry per Layer inside this composition whose composition
  // field is set.
  std::vector<std::unique_ptr<PAGComposition>> childCompositions = {};

  void buildSubtree();
  void spawnTimelines(const std::vector<std::unique_ptr<Timeline>>& drivers);
  void buildChildCompositions();

  // Resolves a hit tgfx layer to its PAGX Layer node by probing this composition's reverse map,
  // then recursively every descendant composition's reverse map. Returns nullptr if no composition
  // in this subtree owns the layer.
  const Node* resolveHitNode(const tgfx::Layer* hitLayer);
};

}  // namespace pagx
