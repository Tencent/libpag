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
#include "pagx/runtime/AnimContext.h"
#include "pagx/runtime/ChannelRegistry.h"
#include "pagx/runtime/PAGFileInternal.h"
#include "pagx/runtime/PAGSurfaceImpl.h"
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
  file->layerTree->tree = LayerBuilder::BuildWithMap(document.get());
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
  auto timeline = std::shared_ptr<PAGTimeline>(
      new PAGTimeline(std::weak_ptr<PAGFile>(shared_from_this()), matched));
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

void PAGFile::onNodesChanged(const std::vector<Node*>& /*dirtyNodes*/) {
  // TODO(PR11): rebuild affected runtime sub-trees and reset relevant timelines.
}

void PAGFile::applyAnimation(Animation* animation, int64_t microseconds, float mix) {
  if (animation == nullptr || document == nullptr || layerTree == nullptr) {
    return;
  }
  if (mix <= 0.0f) {
    return;
  }
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
    AnimContext ctx{&layerTree->tree, targetNode, clampedMix};
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

}  // namespace pagx
