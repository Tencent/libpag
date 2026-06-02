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
#include "pagx/nodes/AnimationTimeline.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Layer.h"

namespace pagx {

std::unique_ptr<PAGComposition> PAGComposition::Make(const Layer* ownerLayer, PAGFile* parentFile) {
  if (ownerLayer == nullptr || parentFile == nullptr || ownerLayer->composition == nullptr) {
    return nullptr;
  }
  auto slot = std::unique_ptr<PAGComposition>(new PAGComposition(ownerLayer, parentFile));
  auto* externalDoc = ownerLayer->externalDoc.get();
  slot->document = externalDoc != nullptr ? externalDoc : parentFile->document.get();
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
  auto buildResult = LayerBuilder::BuildCompositionSubtree(ownerLayer->composition);
  root = std::move(buildResult.root);
  binding = std::move(buildResult.binding);
}

void PAGComposition::spawnTimelines(const std::vector<std::unique_ptr<Timeline>>& drivers) {
  if (parentFile == nullptr || document == nullptr) {
    return;
  }
  for (const auto& driver : drivers) {
    if (driver == nullptr || driver->timelineType() != TimelineType::Animation) {
      continue;
    }
    auto* animationDriver = static_cast<const AnimationTimeline*>(driver.get());
    auto* animation = document->findNode<Animation>(animationDriver->animationId);
    if (animation == nullptr) {
      continue;
    }
    auto timeline = parentFile->createSlotTimeline(animation, &binding, document);
    if (timeline == nullptr) {
      continue;
    }
    if (animationDriver->playing) {
      timeline->play();
    }
    slotTimelines.push_back(std::move(timeline));
  }
}

void PAGComposition::advance(int64_t deltaMicroseconds) {
  for (auto& timeline : slotTimelines) {
    if (timeline != nullptr) {
      timeline->advance(deltaMicroseconds);
    }
  }
  for (auto& child : childSlots) {
    if (child != nullptr) {
      child->advance(deltaMicroseconds);
    }
  }
}

void PAGComposition::apply(float mix) {
  for (auto& timeline : slotTimelines) {
    if (timeline != nullptr) {
      timeline->apply(mix);
    }
  }
  for (auto& child : childSlots) {
    if (child != nullptr) {
      child->apply(mix);
    }
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
      auto container = binding.get<tgfx::Layer>(compLayer);
      if (container != nullptr) {
        container->addChild(childRoot);
      }
    }
    childSlots.push_back(std::move(childSlot));
  }
}

}  // namespace pagx
