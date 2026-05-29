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

#include "pagx/runtime/PAGComposition.h"
#include "pagx/PAGFile.h"
#include "pagx/PAGTimeline.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/AnimationTimeline.h"

namespace pagx {

std::unique_ptr<PAGComposition> PAGComposition::Make(const Layer* ownerLayer,
                                                     PAGFile* parentFile) {
  if (ownerLayer == nullptr || parentFile == nullptr || ownerLayer->composition == nullptr) {
    return nullptr;
  }
  auto slot = std::unique_ptr<PAGComposition>(new PAGComposition(ownerLayer, parentFile));
  // Sealed cross-document wrapper Compositions surface their externalDoc as the effective ID
  // namespace; in-document Compositions fall back to the parent file's document.
  auto* externalDoc = ownerLayer->composition->externalDoc.get();
  slot->effectiveDoc = externalDoc != nullptr ? externalDoc : parentFile->document.get();
  slot->buildSubtree();
  slot->spawnTimelines(ownerLayer->timelines);
  slot->buildChildSlots();
  return slot;
}

PAGComposition::PAGComposition(const Layer* ownerLayer, PAGFile* parentFile)
    : ownerLayer(ownerLayer), parentFile(parentFile) {
}

PAGComposition::~PAGComposition() = default;

void PAGComposition::buildSubtree() {
  layerTree = LayerBuilder::BuildCompositionSubtree(ownerLayer->composition);
}

void PAGComposition::spawnTimelines(const std::vector<std::unique_ptr<Timeline>>& drivers) {
  if (parentFile == nullptr || effectiveDoc == nullptr) {
    return;
  }
  for (const auto& driver : drivers) {
    if (driver == nullptr || driver->timelineType() != TimelineType::Animation) {
      continue;
    }
    auto* animationDriver = static_cast<const AnimationTimeline*>(driver.get());
    auto* animation = effectiveDoc->findNode<Animation>(animationDriver->animationId);
    if (animation == nullptr) {
      continue;
    }
    auto timeline = parentFile->createSlotTimeline(animation, &layerTree, effectiveDoc);
    if (timeline == nullptr) {
      continue;
    }
    if (animationDriver->playing) {
      timeline->play();
    }
    slotTimelines.push_back(std::move(timeline));
  }
}

void PAGComposition::buildChildSlots() {
  if (ownerLayer == nullptr || ownerLayer->composition == nullptr) {
    return;
  }
  for (auto* compLayer : ownerLayer->composition->layers) {
    if (compLayer == nullptr || compLayer->composition == nullptr) {
      continue;
    }
    auto childSlot = PAGComposition::Make(compLayer, parentFile);
    if (childSlot == nullptr) {
      continue;
    }
    auto childRoot = childSlot->rootLayer();
    if (childRoot != nullptr) {
      auto it = layerTree.layerMap.find(compLayer);
      if (it != layerTree.layerMap.end()) {
        it->second->addChild(childRoot);
      }
    }
    childSlots.push_back(std::move(childSlot));
  }
}

}  // namespace pagx
