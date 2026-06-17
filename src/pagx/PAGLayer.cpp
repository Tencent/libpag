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

#include "pagx/PAGLayer.h"
#include "pagx/PAGScene.h"
#include "pagx/nodes/Layer.h"
#include "renderer/LayerBuilder.h"
#include "renderer/ToTGFX.h"
#include "tgfx/layers/Layer.h"

namespace pagx {

PAGLayer::PAGLayer(const Layer* node, std::shared_ptr<tgfx::Layer> runtimeLayer,
                   const std::shared_ptr<PAGScene>& scene)
    : node(node), runtimeLayer(std::move(runtimeLayer)), rootScene(scene) {
  if (scene != nullptr && this->runtimeLayer != nullptr) {
    scene->layerRegistry[this->runtimeLayer.get()] = this;
  }
}

PAGLayer::~PAGLayer() {
  auto scene = rootScene.lock();
  if (scene != nullptr && runtimeLayer != nullptr) {
    scene->layerRegistry.erase(runtimeLayer.get());
  }
}

LayerType PAGLayer::layerType() const {
  return LayerType::Layer;
}

std::string PAGLayer::name() const {
  return node != nullptr ? node->name : std::string();
}

std::string PAGLayer::id() const {
  return node != nullptr ? node->id : std::string();
}

Matrix PAGLayer::getGlobalMatrix() const {
  auto scene = rootScene.lock();
  if (runtimeLayer == nullptr || scene == nullptr) {
    return Matrix::Identity();
  }
  Matrix rootToSurface = {};
  if (!scene->rootToSurfaceMatrix(&rootToSurface)) {
    return Matrix::Identity();
  }
  // local -> root: the runtime layer's transform relative to the tree root. The tree may not be
  // attached to a display list (no draw() yet), so use the scene's root tgfx layer directly rather
  // than tgfx::Layer::root() which is null for a detached subtree.
  auto rootComp = scene->rootComposition();
  auto* rootLayer = rootComp != nullptr ? rootComp->runtimeLayer.get() : nullptr;
  auto localToRoot = runtimeLayer->getRelativeMatrix(rootLayer);
  auto localToSurface = ToTGFX(rootToSurface) * localToRoot;
  return {localToSurface.getScaleX(),     localToSurface.getSkewY(),
          localToSurface.getSkewX(),      localToSurface.getScaleY(),
          localToSurface.getTranslateX(), localToSurface.getTranslateY()};
}

bool PAGLayer::hitTestPoint(float surfaceX, float surfaceY, bool pixelHitTest) {
  auto scene = rootScene.lock();
  if (runtimeLayer == nullptr || scene == nullptr) {
    return false;
  }
  float rootX = 0;
  float rootY = 0;
  if (!scene->surfaceToRoot(surfaceX, surfaceY, &rootX, &rootY)) {
    return false;
  }
  return runtimeLayer->hitTestPoint(rootX, rootY, pixelHitTest);
}

Rect PAGLayer::getBounds() const {
  if (runtimeLayer == nullptr) {
    return {};
  }
  // Bounds relative to the layer's own coordinate space, i.e. before its own transform is applied.
  auto bounds = runtimeLayer->getBounds(runtimeLayer.get());
  return FromTGFX(bounds);
}

void PAGLayer::advance(int64_t deltaMicroseconds) {
  for (auto& child : children) {
    child->advance(deltaMicroseconds);
  }
}

void PAGLayer::apply(float mix) {
  for (auto& child : children) {
    child->apply(mix);
  }
}

void PAGLayer::forEachComposition(void (*visitor)(PAGComposition*)) {
  for (auto& child : children) {
    if (child != nullptr) {
      child->forEachComposition(visitor);
    }
  }
}

}  // namespace pagx
