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
#include "pagx/PAGLayer.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/Layer.h"
#include "pagx/runtime/Drawable.h"
#include "pagx/runtime/PAGComposition.h"
#include "pagx/types/Matrix.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/layers/DisplayList.h"

namespace pagx {

struct PAGFile::FileStorage {
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
  // The file is itself the root composition; point its base parentFile at the file so hit-test
  // handles built from the root subtree can reach the file for surface coordinate conversion.
  file->composition->parentFile = file.get();
  file->fileStorage = std::make_unique<FileStorage>();
  file->displayOptions = std::unique_ptr<PAGDisplayOptions>(new PAGDisplayOptions(file));
  auto buildResult = LayerBuilder::BuildWithSlotsHandedOff(document.get());
  file->composition->root = std::move(buildResult.root);
  file->composition->binding = std::move(buildResult.binding);
  // Build runtime compositions: each top-level Layer with composition!=null gets its own
  // PAGComposition instance, which constructs a per-instance subtree and spawns timeline drivers.
  // These become the file root's child compositions in the PAGComposition base.
  for (auto* layer : document->layers) {
    if (layer == nullptr || layer->composition == nullptr) {
      continue;
    }
    auto composition = PAGComposition::MakeChild(layer, file.get());
    if (composition == nullptr) {
      continue;
    }
    auto compositionRoot = composition->composition->root;
    if (compositionRoot != nullptr) {
      auto container = file->composition->binding.get<tgfx::Layer>(layer);
      if (container != nullptr) {
        container->addChild(compositionRoot);
      }
    }
    file->composition->childCompositions.push_back(std::move(composition));
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
      std::shared_ptr<PAGTimeline>(new PAGTimeline(matched, &composition->binding, document.get()));
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
  if (composition->root == nullptr) {
    return false;
  }
  auto& drawable = surface->drawable;
  if (!fileStorage->rootAttached) {
    fileStorage->displayList.root()->addChild(composition->root);
    fileStorage->rootAttached = true;
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
  fileStorage->displayList.render(tgfxSurface.get());
  drawable->present(context);
  device->unlock();
  return true;
}

float PAGFile::width() const {
  return document != nullptr ? document->width : 0.0f;
}

float PAGFile::height() const {
  return document != nullptr ? document->height : 0.0f;
}

PAGDisplayOptions* PAGFile::getDisplayOptions() const {
  return displayOptions.get();
}

void PAGFile::advanceAndApply(int64_t deltaMicroseconds) {
  advance(deltaMicroseconds);
  apply();
}

std::vector<std::shared_ptr<PAGLayer>> PAGFile::getLayersUnderPoint(float surfaceX,
                                                                    float surfaceY) {
  float rootX = 0;
  float rootY = 0;
  if (!surfaceToRoot(surfaceX, surfaceY, &rootX, &rootY)) {
    return {};
  }
  return PAGComposition::getLayersUnderPoint(rootX, rootY);
}

bool PAGFile::surfaceToRoot(float surfaceX, float surfaceY, float* rootX, float* rootY) const {
  if (fileStorage == nullptr || rootX == nullptr || rootY == nullptr) {
    return false;
  }
  float zoomScale = fileStorage->displayList.zoomScale();
  if (zoomScale == 0.0f) {
    return false;
  }
  const auto& contentOffset = fileStorage->displayList.contentOffset();
  *rootX = (surfaceX - contentOffset.x) / zoomScale;
  *rootY = (surfaceY - contentOffset.y) / zoomScale;
  return true;
}

bool PAGFile::rootToSurfaceMatrix(Matrix* out) const {
  if (fileStorage == nullptr || out == nullptr) {
    return false;
  }
  float zoomScale = fileStorage->displayList.zoomScale();
  const auto& contentOffset = fileStorage->displayList.contentOffset();
  // surface = zoomScale * root + contentOffset.
  *out = {zoomScale, 0, 0, zoomScale, contentOffset.x, contentOffset.y};
  return true;
}

void* PAGFile::rootRuntimeLayer() const {
  return composition != nullptr ? composition->root.get() : nullptr;
}

void PAGFile::onNodesChanged(const std::vector<Node*>& /*dirtyNodes*/) {
  // TODO(PR11): rebuild affected runtime sub-trees and reset relevant timelines.
}

RuntimeBinding* PAGFile::mutableBinding() {
  return &composition->binding;
}

void* PAGFile::getDisplayListForOptions() const {
  return fileStorage != nullptr ? &fileStorage->displayList : nullptr;
}

}  // namespace pagx
