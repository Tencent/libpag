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
#include "pagx/PAGFile.h"
#include "pagx/nodes/Layer.h"
#include "renderer/ToTGFX.h"
#include "tgfx/layers/Layer.h"

namespace pagx {

struct PAGLayer::Impl {
  const Layer* node = nullptr;
  tgfx::Layer* runtimeLayer = nullptr;
  PAGFile* rootFile = nullptr;
};

PAGLayer::PAGLayer() : impl(std::make_unique<Impl>()) {
}

PAGLayer::~PAGLayer() = default;

std::shared_ptr<PAGLayer> PAGLayer::Wrap(const Layer* node, const void* runtimeLayer,
                                         PAGFile* rootFile) {
  if (node == nullptr) {
    return nullptr;
  }
  auto layer = std::shared_ptr<PAGLayer>(new PAGLayer());
  layer->impl->node = node;
  layer->impl->runtimeLayer =
      const_cast<tgfx::Layer*>(static_cast<const tgfx::Layer*>(runtimeLayer));
  layer->impl->rootFile = rootFile;
  return layer;
}

std::string PAGLayer::getName() const {
  return impl->node != nullptr ? impl->node->name : std::string();
}

Matrix PAGLayer::getGlobalMatrix() const {
  if (impl->runtimeLayer == nullptr || impl->rootFile == nullptr) {
    return Matrix::Identity();
  }
  Matrix rootToSurface = {};
  if (!impl->rootFile->rootToSurfaceMatrix(&rootToSurface)) {
    return Matrix::Identity();
  }
  // local -> root: the runtime layer's transform relative to the tree root. The tree may not be
  // attached to a display list (no draw() yet), so use the file's root tgfx layer directly rather
  // than tgfx::Layer::root() which is null for a detached subtree.
  auto* rootLayer = static_cast<tgfx::Layer*>(impl->rootFile->rootRuntimeLayer());
  auto localToRoot = impl->runtimeLayer->getRelativeMatrix(rootLayer);
  auto localToSurface = ToTGFX(rootToSurface) * localToRoot;
  return {localToSurface.getScaleX(),     localToSurface.getSkewY(),
          localToSurface.getSkewX(),      localToSurface.getScaleY(),
          localToSurface.getTranslateX(), localToSurface.getTranslateY()};
}

bool PAGLayer::hitTestPoint(float surfaceX, float surfaceY, bool shapeHitTest) {
  if (impl->runtimeLayer == nullptr || impl->rootFile == nullptr) {
    return false;
  }
  float rootX = 0;
  float rootY = 0;
  if (!impl->rootFile->surfaceToRoot(surfaceX, surfaceY, &rootX, &rootY)) {
    return false;
  }
  return impl->runtimeLayer->hitTestPoint(rootX, rootY, shapeHitTest);
}

}  // namespace pagx
