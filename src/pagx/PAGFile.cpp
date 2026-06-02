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
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/Layer.h"
#include "pagx/runtime/Drawable.h"
#include "pagx/runtime/PAGComposition.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/layers/DisplayList.h"

namespace pagx {

struct PAGFile::LayerTreeStorage {
  std::shared_ptr<tgfx::Layer> root = nullptr;
  RuntimeBinding binding = {};
  tgfx::DisplayList displayList;
  bool rootAttached = false;
};

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
  file->displayOptions = std::unique_ptr<PAGDisplayOptions>(new PAGDisplayOptions(file));
  auto buildResult = LayerBuilder::BuildWithSlotsHandedOff(document.get());
  file->layerTree->root = std::move(buildResult.root);
  file->layerTree->binding = std::move(buildResult.binding);
  // Build runtime compositions: each top-level Layer with composition!=null gets its own
  // PAGComposition instance, which constructs a per-instance subtree and spawns timeline drivers.
  for (auto* layer : document->layers) {
    if (layer == nullptr || layer->composition == nullptr) {
      continue;
    }
    auto composition = PAGComposition::Make(layer, file.get());
    if (composition == nullptr) {
      continue;
    }
    auto compositionRoot = composition->rootLayer();
    if (compositionRoot != nullptr) {
      auto container = file->layerTree->binding.get<tgfx::Layer>(layer);
      if (container != nullptr) {
        container->addChild(compositionRoot);
      }
    }
    file->compositions.push_back(std::move(composition));
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
  // Top-level timelines target the file's top-level runtime binding directly. Channel target IDs
  // are resolved against the primary document.
  auto timeline =
      std::shared_ptr<PAGTimeline>(new PAGTimeline(matched, &layerTree->binding, document.get()));
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
  if (surface == nullptr || surface->drawable == nullptr) {
    return false;
  }
  if (layerTree == nullptr || layerTree->root == nullptr) {
    return false;
  }
  auto& drawable = surface->drawable;
  if (!layerTree->rootAttached) {
    layerTree->displayList.root()->addChild(layerTree->root);
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

PAGDisplayOptions* PAGFile::getDisplayOptions() const {
  return displayOptions.get();
}

void PAGFile::advance(int64_t deltaMicroseconds) {
  // Drives only the timelines spawned by Layer.timelines drivers. Each runtime composition advances
  // its own timelines and recursively advances its child compositions. Top-level animations are not
  // driven here; callers advance them explicitly via getTimeline(...).
  for (auto& composition : compositions) {
    if (composition != nullptr) {
      composition->advance(deltaMicroseconds);
    }
  }
}

void PAGFile::apply() {
  for (auto& composition : compositions) {
    if (composition != nullptr) {
      composition->apply(1.0f);
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

std::shared_ptr<PAGTimeline> PAGFile::createCompositionTimeline(Animation* animation,
                                                                RuntimeBinding* binding,
                                                                PAGXDocument* contextDoc) {
  if (animation == nullptr || binding == nullptr) {
    return nullptr;
  }
  auto* effectiveDoc = contextDoc != nullptr ? contextDoc : document.get();
  return std::shared_ptr<PAGTimeline>(new PAGTimeline(animation, binding, effectiveDoc));
}

RuntimeBinding* PAGFile::mutableBinding() {
  return layerTree != nullptr ? &layerTree->binding : nullptr;
}

void* PAGFile::getDisplayListForOptions() const {
  return layerTree != nullptr ? &layerTree->displayList : nullptr;
}

}  // namespace pagx
