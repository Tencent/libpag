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

#include "pagx/PAGScene.h"
#include "pagx/PAGLayer.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/Layer.h"
#include "pagx/runtime/Drawable.h"
#include "pagx/types/Matrix.h"
#include "renderer/LayerBuilder.h"
#include "renderer/ToTGFX.h"
#include "tgfx/layers/DisplayList.h"

namespace pagx {

PAGScene::PAGScene() = default;

std::shared_ptr<PAGScene> PAGScene::Make(std::shared_ptr<PAGXDocument> document) {
  if (document == nullptr) {
    return nullptr;
  }
  if (!document->isLayoutApplied()) {
    document->applyLayout();
  }
  auto scene = std::shared_ptr<PAGScene>(new PAGScene());
  scene->document = document;
  scene->displayList = std::make_unique<tgfx::DisplayList>();
  scene->displayOptions = std::unique_ptr<PAGDisplayOptions>(new PAGDisplayOptions(scene));
  auto buildResult = LayerBuilder::BuildForRuntime(document.get());
  auto rootComp = std::shared_ptr<PAGComposition>(
      new PAGComposition(nullptr, std::move(buildResult.root), scene.get()));
  *rootComp->binding = std::move(buildResult.binding);
  rootComp->document = document.get();
  scene->_rootComposition = rootComp;
  scene->_rootComposition->buildChildren(document->layers);
  scene->displayList->root()->addChild(rootComp->runtimeLayer);
  document->registerLiveScene(scene);
  return scene;
}

PAGScene::~PAGScene() {
  if (document != nullptr) {
    document->unregisterLiveScene(this);
  }
}

std::vector<std::string> PAGScene::getTimelineIds() const {
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

std::shared_ptr<PAGTimeline> PAGScene::getTimeline(const std::string& id) {
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
      new PAGTimeline(matched, _rootComposition->binding.get(), document.get()));
  timelinesByAnimation.emplace(matched, timeline);
  return timeline;
}

std::shared_ptr<PAGTimeline> PAGScene::getDefaultTimeline() {
  if (document == nullptr || document->animations.empty()) {
    return nullptr;
  }
  auto* first = document->animations.front();
  if (first == nullptr) {
    return nullptr;
  }
  return getTimeline(first->id);
}

std::shared_ptr<PAGComposition> PAGScene::rootComposition() const {
  return _rootComposition;
}

bool PAGScene::draw(const std::shared_ptr<PAGSurface>& surface) {
  if (surface == nullptr || surface->drawable == nullptr) {
    return false;
  }
  if (_rootComposition == nullptr || _rootComposition->runtimeLayer == nullptr) {
    return false;
  }
  auto& drawable = surface->drawable;
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
  displayList->render(tgfxSurface.get());
  drawable->present(context);
  device->unlock();
  return true;
}

float PAGScene::width() const {
  return document != nullptr ? document->width : 0.0f;
}

float PAGScene::height() const {
  return document != nullptr ? document->height : 0.0f;
}

PAGDisplayOptions* PAGScene::getDisplayOptions() const {
  return displayOptions.get();
}

void PAGScene::advanceAndApply(int64_t deltaMicroseconds) {
  if (_rootComposition != nullptr) {
    _rootComposition->advance(deltaMicroseconds);
    _rootComposition->apply();
  }
}

std::vector<std::shared_ptr<PAGLayer>> PAGScene::getLayersUnderPoint(float surfaceX,
                                                                     float surfaceY) {
  float rootX = 0;
  float rootY = 0;
  if (!surfaceToRoot(surfaceX, surfaceY, &rootX, &rootY)) {
    return {};
  }
  if (_rootComposition != nullptr) {
    return _rootComposition->getLayersUnderPoint(rootX, rootY);
  }
  return {};
}

Rect PAGScene::getGlobalBounds(const std::shared_ptr<PAGLayer>& pagLayer) const {
  if (pagLayer == nullptr || pagLayer->runtimeLayer == nullptr || pagLayer->rootScene != this) {
    return {};
  }
  auto* rootLayer = static_cast<tgfx::Layer*>(rootRuntimeLayer());
  auto rootBounds = pagLayer->runtimeLayer->getBounds(rootLayer);
  Matrix rootToSurface = {};
  rootToSurfaceMatrix(&rootToSurface);
  auto surfaceBounds = ToTGFX(rootToSurface).mapRect(rootBounds);
  return FromTGFX(surfaceBounds);
}

bool PAGScene::surfaceToRoot(float surfaceX, float surfaceY, float* rootX, float* rootY) const {
  if (rootX == nullptr || rootY == nullptr) {
    return false;
  }
  float zoomScale = displayList->zoomScale();
  if (zoomScale == 0.0f) {
    return false;
  }
  const auto& contentOffset = displayList->contentOffset();
  *rootX = (surfaceX - contentOffset.x) / zoomScale;
  *rootY = (surfaceY - contentOffset.y) / zoomScale;
  return true;
}

bool PAGScene::rootToSurfaceMatrix(Matrix* out) const {
  if (out == nullptr) {
    return false;
  }
  float zoomScale = displayList->zoomScale();
  const auto& contentOffset = displayList->contentOffset();
  *out = {zoomScale, 0, 0, zoomScale, contentOffset.x, contentOffset.y};
  return true;
}

void* PAGScene::rootRuntimeLayer() const {
  return _rootComposition != nullptr ? _rootComposition->runtimeLayer.get() : nullptr;
}

void PAGScene::onNodesChanged(const std::vector<Node*>& /*dirtyNodes*/) {
  // TODO(PR11): rebuild affected runtime sub-trees and reset relevant timelines.
}

RuntimeBinding* PAGScene::mutableBinding() {
  return _rootComposition != nullptr ? _rootComposition->binding.get() : nullptr;
}

tgfx::DisplayList* PAGScene::getDisplayListForOptions() const {
  return displayList.get();
}

}  // namespace pagx
