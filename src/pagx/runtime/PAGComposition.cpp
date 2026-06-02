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
#include "pagx/PAGLayer.h"
#include "pagx/PAGTimeline.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationTimeline.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Layer.h"

namespace pagx {

void PAGComposition::Impl::buildSubtree() {
  auto buildResult = LayerBuilder::BuildCompositionSubtree(ownerLayer->composition);
  root = std::move(buildResult.root);
  binding = std::move(buildResult.binding);
}

void PAGComposition::Impl::spawnTimelines(const std::vector<std::unique_ptr<Timeline>>& drivers) {
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
    auto timeline = parentFile->createCompositionTimeline(animation, &binding, document);
    if (timeline == nullptr) {
      continue;
    }
    if (animationDriver->playing) {
      timeline->play();
    }
    timelines.push_back(std::move(timeline));
  }
}

void PAGComposition::Impl::buildChildCompositions() {
  if (ownerLayer == nullptr || ownerLayer->composition == nullptr) {
    return;
  }
  for (auto* compLayer : ownerLayer->composition->layers) {
    if (compLayer == nullptr || compLayer->composition == nullptr) {
      continue;
    }
    auto childComposition = PAGComposition::MakeChild(compLayer, parentFile);
    if (childComposition == nullptr) {
      continue;
    }
    auto childRoot = childComposition->composition->root;
    if (childRoot != nullptr) {
      auto container = binding.get<tgfx::Layer>(compLayer);
      if (container != nullptr) {
        container->addChild(childRoot);
      }
    }
    childCompositions.push_back(std::move(childComposition));
  }
}

std::unique_ptr<PAGComposition> PAGComposition::MakeChild(const Layer* ownerLayer,
                                                          PAGFile* parentFile) {
  if (ownerLayer == nullptr || parentFile == nullptr || ownerLayer->composition == nullptr) {
    return nullptr;
  }
  auto composition = std::unique_ptr<PAGComposition>(new PAGComposition());
  auto* impl = composition->composition.get();
  impl->ownerLayer = ownerLayer;
  impl->parentFile = parentFile;
  auto* externalDoc = ownerLayer->externalDoc.get();
  impl->document = externalDoc != nullptr ? externalDoc : parentFile->document.get();
  impl->buildSubtree();
  impl->spawnTimelines(ownerLayer->timelines);
  impl->buildChildCompositions();
  return composition;
}

PAGComposition::PAGComposition() : composition(std::make_unique<Impl>()) {
}

PAGComposition::~PAGComposition() = default;

void PAGComposition::advance(int64_t deltaMicroseconds) {
  for (auto& timeline : composition->timelines) {
    if (timeline != nullptr) {
      timeline->advance(deltaMicroseconds);
    }
  }
  for (auto& child : composition->childCompositions) {
    if (child != nullptr) {
      child->advance(deltaMicroseconds);
    }
  }
}

void PAGComposition::apply(float mix) {
  for (auto& timeline : composition->timelines) {
    if (timeline != nullptr) {
      timeline->apply(mix);
    }
  }
  for (auto& child : composition->childCompositions) {
    if (child != nullptr) {
      child->apply(mix);
    }
  }
}

const Node* PAGComposition::Impl::resolveHitNode(const tgfx::Layer* hitLayer) {
  const Node* node = binding.nodeForLayer(hitLayer);
  if (node != nullptr) {
    return node;
  }
  for (auto& child : childCompositions) {
    if (child == nullptr) {
      continue;
    }
    node = child->composition->resolveHitNode(hitLayer);
    if (node != nullptr) {
      return node;
    }
  }
  return nullptr;
}

std::vector<std::shared_ptr<PAGLayer>> PAGComposition::getLayersUnderPoint(float x, float y) {
  std::vector<std::shared_ptr<PAGLayer>> result = {};
  if (composition->root == nullptr) {
    return result;
  }
  // tgfx getLayersUnderPoint returns the hit tgfx layers top-most first, in this composition's root
  // coordinate space, covering the whole subtree (child composition roots are attached into the
  // same tree). A hit tgfx layer may be an internal sub-layer (mask, vector content) not registered
  // as a node's primary layer; we keep only those that resolve to a PAGX Layer node. Resolution
  // probes this composition's reverse map and recurses into descendant compositions, since each
  // composition instance owns the binding for the subtree it built. The first array entry stays the
  // top-most layer.
  auto hitLayers = composition->root->getLayersUnderPoint(x, y);
  for (const auto& hitLayer : hitLayers) {
    const Node* node = composition->resolveHitNode(hitLayer.get());
    if (node != nullptr && node->nodeType() == NodeType::Layer) {
      auto handle =
          PAGLayer::Wrap(static_cast<const Layer*>(node), hitLayer.get(), composition->parentFile);
      if (handle != nullptr) {
        result.push_back(std::move(handle));
      }
    }
  }
  return result;
}

}  // namespace pagx
