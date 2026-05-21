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

#include "pagx/PAGFile.h"
#include <algorithm>
#include "pagx/animation/Animation.h"
#include "pagx/animation/AnimationObject.h"
#include "pagx/nodes/Layer.h"
#include "pagx/runtime/AnimContext.h"
#include "pagx/runtime/ChannelRegistry.h"
#include "pagx/runtime/PAGComposition.h"
#include "pagx/runtime/PAGFileInternal.h"
#include "pagx/runtime/PAGSurfaceImpl.h"
#include "pagx/timeline/AnimationTimeline.h"
#include "renderer/LayerBuilder.h"

namespace pagx {

std::shared_ptr<PAGFile> PAGFile::Make(std::shared_ptr<PAGXDocument> document) {
  if (document == nullptr) {
    return nullptr;
  }
  if (!document->isLayoutApplied()) {
    document->applyLayout();
  }
  auto file = std::shared_ptr<PAGFile>(new PAGFile());
  file->document = document;
  file->layerTree = std::make_unique<LayerTreeStorage>();
  file->layerTree->tree = LayerBuilder::BuildWithSlotsHandedOff(document.get());
  // Build runtime composition slots: each top-level Layer with composition!=null gets its own
  // PAGComposition instance, which constructs a per-slot subtree and spawns timeline drivers.
  for (auto* layer : document->layers) {
    if (layer == nullptr || layer->composition == nullptr) {
      continue;
    }
    auto slot = PAGComposition::Make(layer, file.get());
    if (slot == nullptr) {
      continue;
    }
    auto slotRoot = slot->rootLayer();
    if (slotRoot != nullptr) {
      auto it = file->layerTree->tree.layerMap.find(layer);
      if (it != file->layerTree->tree.layerMap.end()) {
        it->second->addChild(slotRoot);
      }
    }
    file->compositionSlots.push_back(std::move(slot));
  }
  // PR1 only registers the file itself; node-level reverse mapping is populated when notifyChange
  // dispatch is wired up (PR11). Pass an empty referenced-node list for now.
  document->registerLiveFile(file, {});
  return file;
}

PAGFile::~PAGFile() {
  if (document != nullptr) {
    document->unregisterLiveFile(this);
  }
}

std::vector<std::string> PAGFile::getTimelineIds() const {
  std::vector<std::string> ids = {};
  if (document == nullptr) {
    return ids;
  }
  ids.reserve(document->animations.size());
  for (auto* animation : document->animations) {
    if (animation != nullptr) {
      ids.push_back(animation->id);
    }
  }
  return ids;
}

std::shared_ptr<PAGTimeline> PAGFile::getTimeline(const std::string& id) {
  if (document == nullptr) {
    return nullptr;
  }
  Animation* matched = nullptr;
  for (auto* animation : document->animations) {
    if (animation != nullptr && animation->id == id) {
      matched = animation;
      break;
    }
  }
  if (matched == nullptr) {
    return nullptr;
  }
  auto it = timelinesByAnimation.find(matched);
  if (it != timelinesByAnimation.end()) {
    return it->second;
  }
  // Top-level timelines target the file's top-level layerTree directly.
  auto timeline = std::shared_ptr<PAGTimeline>(new PAGTimeline(
      std::weak_ptr<PAGFile>(shared_from_this()), matched, &layerTree->tree));
  timelinesByAnimation.emplace(matched, timeline);
  return timeline;
}

std::shared_ptr<PAGTimeline> PAGFile::getDefaultTimeline() {
  if (document == nullptr || document->animations.empty()) {
    return nullptr;
  }
  auto* first = document->animations.front();
  if (first == nullptr) {
    return nullptr;
  }
  return getTimeline(first->id);
}

bool PAGFile::draw(const std::shared_ptr<PAGSurface>& surface) {
  if (surface == nullptr || surface->impl == nullptr || surface->impl->drawable == nullptr) {
    return false;
  }
  if (layerTree == nullptr || layerTree->tree.root == nullptr) {
    return false;
  }
  auto& drawable = surface->impl->drawable;
  if (!layerTree->rootAttached) {
    layerTree->displayList.root()->addChild(layerTree->tree.root);
    layerTree->rootAttached = true;
  }
  auto device = drawable->getDevice();
  if (device == nullptr) {
    return false;
  }
  auto* context = device->lockContext();
  if (context == nullptr) {
    return false;
  }
  auto tgfxSurface = drawable->getSurface(context);
  if (tgfxSurface == nullptr) {
    device->unlock();
    return false;
  }
  layerTree->displayList.render(tgfxSurface.get());
  drawable->present(context);
  device->unlock();
  return true;
}

float PAGFile::getWidth() const {
  return document != nullptr ? document->width : 0.0f;
}

float PAGFile::getHeight() const {
  return document != nullptr ? document->height : 0.0f;
}

void PAGFile::advance(int64_t deltaMicroseconds) {
  // Default top-level timeline acts as master clock.
  auto defaultTimeline = getDefaultTimeline();
  if (defaultTimeline != nullptr) {
    defaultTimeline->advance(deltaMicroseconds);
  }
  // Slot timelines spawned by Layer.timelines drivers follow the master clock.
  for (auto& slot : compositionSlots) {
    if (slot == nullptr) {
      continue;
    }
    for (auto& timeline : slot->timelines()) {
      if (timeline != nullptr) {
        timeline->advance(deltaMicroseconds);
      }
    }
  }
}

void PAGFile::apply() {
  auto defaultTimeline = getDefaultTimeline();
  if (defaultTimeline != nullptr) {
    defaultTimeline->apply(1.0f);
  }
  for (auto& slot : compositionSlots) {
    if (slot == nullptr) {
      continue;
    }
    for (auto& timeline : slot->timelines()) {
      if (timeline != nullptr) {
        timeline->apply(1.0f);
      }
    }
  }
}

void PAGFile::advanceAndApply(int64_t deltaMicroseconds) {
  advance(deltaMicroseconds);
  apply();
}

void PAGFile::onNodesChanged(const std::vector<Node*>& /*dirtyNodes*/) {
  // TODO(PR11): rebuild affected runtime sub-trees and reset relevant timelines.
}

void PAGFile::applyAnimation(Animation* animation, PAGLayerTree* slotLayerTree,
                             int64_t microseconds, float mix) {
  if (animation == nullptr || document == nullptr || layerTree == nullptr) {
    return;
  }
  if (mix <= 0.0f) {
    return;
  }
  auto* targetTree = slotLayerTree != nullptr ? slotLayerTree : &layerTree->tree;
  auto clampedMix = std::min(1.0f, mix);
  const auto& registry = ChannelRegistry::Get();
  for (auto* object : animation->objects) {
    if (object == nullptr) {
      continue;
    }
    auto* targetNode = document->findNode(object->target);
    if (targetNode == nullptr) {
      continue;
    }
    AnimContext ctx{targetTree, targetNode, clampedMix};
    for (auto* property : object->properties) {
      if (property == nullptr) {
        continue;
      }
      auto* desc = registry.find(targetNode->nodeType(), property->channel);
      if (desc == nullptr || !desc->writer) {
        continue;
      }
      desc->writer(ctx, property->evaluateAt(microseconds, animation->frameRate));
    }
  }
}

std::shared_ptr<PAGTimeline> PAGFile::createSlotTimeline(Animation* animation,
                                                         PAGLayerTree* layerTreeForSlot) {
  if (animation == nullptr || layerTreeForSlot == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<PAGTimeline>(new PAGTimeline(
      std::weak_ptr<PAGFile>(shared_from_this()), animation, layerTreeForSlot));
}

}  // namespace pagx
