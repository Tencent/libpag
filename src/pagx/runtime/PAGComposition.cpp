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

#include "pagx/PAGComposition.h"
#include "pagx/PAGLayer.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGTimeline.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationTimeline.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Layer.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/layers/Layer.h"

namespace pagx {

PAGComposition::PAGComposition(const Layer* node, std::shared_ptr<tgfx::Layer> runtimeLayer,
                               const std::shared_ptr<PAGScene>& scene)
    : PAGLayer(node, std::move(runtimeLayer), scene),
      binding(std::make_unique<RuntimeBinding>()) {
}

PAGComposition::~PAGComposition() = default;

LayerType PAGComposition::layerType() const {
  return LayerType::Composition;
}

std::shared_ptr<PAGComposition> PAGComposition::MakeChild(const Layer* ownerLayer,
                                                          const std::shared_ptr<PAGScene>& parentScene) {
  if (ownerLayer == nullptr || parentScene == nullptr || ownerLayer->composition == nullptr) {
    return nullptr;
  }
  auto buildResult = LayerBuilder::BuildCompositionSubtree(ownerLayer->composition);
  auto composition = std::shared_ptr<PAGComposition>(
      new PAGComposition(ownerLayer, std::move(buildResult.root), parentScene));
  *composition->binding = std::move(buildResult.binding);
  auto* externalDoc = ownerLayer->externalDoc.get();
  composition->document = externalDoc != nullptr ? externalDoc : parentScene->document.get();
  // Spawn the timelines declared on the owner layer, targeting this composition's own binding and
  // document, then build the persistent per-layer runtime node tree for the composition content.
  for (const auto& driver : ownerLayer->timelines) {
    if (driver == nullptr || driver->timelineType() != TimelineType::Animation) {
      continue;
    }
    auto* animationDriver = static_cast<const AnimationTimeline*>(driver.get());
    auto* animation = composition->document != nullptr
                          ? composition->document->findNode<Animation>(animationDriver->animationId)
                          : nullptr;
    if (animation == nullptr) {
      continue;
    }
    auto timeline = std::shared_ptr<PAGTimeline>(
        new PAGTimeline(animation, composition->binding.get(), composition->document));
    if (animationDriver->playing) {
      timeline->play();
    }
    composition->timelines.push_back(std::move(timeline));
  }
  composition->buildChildren(ownerLayer->composition->layers);
  return composition;
}

void PAGComposition::buildChildren(const std::vector<Layer*>& layers) {
  auto scene = rootScene.lock();
  if (!scene) {
    return;
  }
  for (auto* layer : layers) {
    if (layer == nullptr) {
      continue;
    }
    auto layerRuntime = binding->get<tgfx::Layer>(layer);
    if (layer->composition != nullptr) {
      auto childComposition = PAGComposition::MakeChild(layer, scene);
      if (childComposition == nullptr) {
        continue;
      }
      if (childComposition->runtimeLayer != nullptr && layerRuntime != nullptr) {
        layerRuntime->addChild(childComposition->runtimeLayer);
      }
      children.push_back(std::move(childComposition));
    } else {
      children.push_back(std::shared_ptr<PAGLayer>(new PAGLayer(layer, layerRuntime, scene)));
    }
  }
}

void PAGComposition::advance(int64_t deltaMicroseconds) {
  for (auto& timeline : timelines) {
    if (timeline != nullptr) {
      timeline->advance(deltaMicroseconds);
    }
  }
  for (auto& child : children) {
    if (child != nullptr && child->layerType() != LayerType::Layer) {
      static_cast<PAGComposition*>(child.get())->advance(deltaMicroseconds);
    }
  }
}

void PAGComposition::apply(float mix) {
  for (auto& timeline : timelines) {
    if (timeline != nullptr) {
      timeline->apply(mix);
    }
  }
  for (auto& child : children) {
    if (child != nullptr && child->layerType() != LayerType::Layer) {
      static_cast<PAGComposition*>(child.get())->apply(mix);
    }
  }
}

std::shared_ptr<PAGLayer> PAGComposition::findChildForLayer(const tgfx::Layer* hitLayer) {
  for (auto& child : children) {
    if (child == nullptr) {
      continue;
    }
    if (child->runtimeLayer.get() == hitLayer) {
      return child;
    }
    if (child->layerType() != LayerType::Layer) {
      auto found = static_cast<PAGComposition*>(child.get())->findChildForLayer(hitLayer);
      if (found != nullptr) {
        return found;
      }
    }
  }
  return nullptr;
}

std::vector<std::shared_ptr<PAGLayer>> PAGComposition::getLayersUnderPoint(float x, float y) {
  std::vector<std::shared_ptr<PAGLayer>> result = {};
  if (runtimeLayer == nullptr) {
    return result;
  }
  // tgfx getLayersUnderPoint returns the hit tgfx layers top-most first, in this composition's root
  // coordinate space, covering the whole subtree (child composition roots are attached into the
  // same tree). A hit tgfx layer may be an internal sub-layer (mask, vector content) not bound to a
  // persistent node; keep only those that match a persistent PAGLayer node's runtimeLayer.
  // Resolution searches this composition's children and recurses into descendant compositions. The
  // first array entry stays the top-most layer.
  auto hitLayers = runtimeLayer->getLayersUnderPoint(x, y);
  for (const auto& hitLayer : hitLayers) {
    auto matched = findChildForLayer(hitLayer.get());
    if (matched != nullptr && matched->layerType() == LayerType::Layer) {
      result.push_back(std::move(matched));
    }
  }
  return result;
}

}  // namespace pagx
