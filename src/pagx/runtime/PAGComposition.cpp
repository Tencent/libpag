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
#include <unordered_map>
#include <unordered_set>
#include "pagx/PAGLayer.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGTimeline.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationTimeline.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Node.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/layers/Layer.h"

namespace pagx {

PAGComposition::PAGComposition(const Layer* node, std::shared_ptr<tgfx::Layer> runtimeLayer,
                               const std::shared_ptr<PAGScene>& scene)
    : PAGLayer(node, std::move(runtimeLayer), scene), binding(std::make_unique<RuntimeBinding>()) {
}

PAGComposition::~PAGComposition() = default;

LayerType PAGComposition::layerType() const {
  return LayerType::Composition;
}

void PAGComposition::advance(int64_t deltaMicroseconds) {
  for (auto& timeline : timelines) {
    timeline->advance(deltaMicroseconds);
  }
  PAGLayer::advance(deltaMicroseconds);
}

void PAGComposition::apply(float mix) {
  for (auto& timeline : timelines) {
    timeline->apply(mix);
  }
  PAGLayer::apply(mix);
}

std::shared_ptr<PAGComposition> PAGComposition::MakeChild(
    const Layer* ownerLayer, const std::shared_ptr<PAGScene>& parentScene,
    std::unordered_set<const Composition*>& visited) {
  if (ownerLayer == nullptr || parentScene == nullptr || ownerLayer->composition == nullptr) {
    return nullptr;
  }
  // Cycle guard: visited acts as a path stack — a composition on the current ancestor chain
  // is rejected, but the same composition reused by a sibling is not (visited is scoped per path).
  // The external-file chain guard (loadFileData) does not cover same-document @id references.
  auto* sourceComposition = ownerLayer->composition;
  if (visited.find(sourceComposition) != visited.end()) {
    auto* document = parentScene->document.get();
    if (document != nullptr) {
      document->errors.push_back("Cyclic composition reference detected: '@" +
                                 sourceComposition->id + "'.");
    }
    return nullptr;
  }
  auto buildResult = LayerBuilder::BuildCompositionSubtree(ownerLayer->composition);
  auto composition = std::shared_ptr<PAGComposition>(
      new PAGComposition(ownerLayer, std::move(buildResult.root), parentScene));
  *composition->binding = std::move(buildResult.binding);
  auto* externalDoc = ownerLayer->externalDoc.get();
  composition->document = externalDoc != nullptr ? externalDoc : parentScene->document.get();
  if (externalDoc != nullptr) {
    externalDoc->registerLiveScene(parentScene);
  }
  composition->spawnTimelines(parentScene);
  visited.insert(sourceComposition);
  composition->buildChildren(ownerLayer->composition->layers, visited);
  visited.erase(sourceComposition);
  return composition;
}

void PAGComposition::spawnTimelines(const std::shared_ptr<PAGScene>& scene) {
  timelines.clear();
  if (node == nullptr) {
    return;
  }
  for (const auto& driver : node->timelines) {
    if (driver == nullptr || driver->timelineType() != TimelineType::Animation) {
      continue;
    }
    auto* animationDriver = static_cast<const AnimationTimeline*>(driver.get());
    auto* animation =
        document != nullptr ? document->findNode<Animation>(animationDriver->animationId) : nullptr;
    if (animation == nullptr) {
      continue;
    }
    auto timeline =
        std::shared_ptr<PAGTimeline>(new PAGTimeline(animation, binding.get(), document, scene));
    if (!animationDriver->playing) {
      timeline->pause();
    }
    timelines.push_back(std::move(timeline));
  }
}

void PAGComposition::spawnTimelinesFromScene() {
  auto scene = rootScene.lock();
  if (scene != nullptr) {
    spawnTimelines(scene);
  }
}

void PAGComposition::resetTimelines() {
  forEachComposition(ResetCompositionTimelines);
}

void PAGComposition::forEachComposition(void (*visitor)(PAGComposition*)) {
  PAGLayer::forEachComposition(visitor);
  visitor(this);
}

void PAGComposition::ResetCompositionTimelines(PAGComposition* comp) {
  comp->spawnTimelinesFromScene();
}

void PAGComposition::buildChildren(const std::vector<Layer*>& layers,
                                   std::unordered_set<const Composition*>& visited) {
  auto scene = rootScene.lock();
  if (!scene) {
    return;
  }
  BuildChildren(binding.get(), layers, children, scene, visited);
}

void PAGComposition::BuildChildren(RuntimeBinding* binding, const std::vector<Layer*>& layers,
                                   std::vector<std::shared_ptr<PAGLayer>>& outChildren,
                                   const std::shared_ptr<PAGScene>& scene,
                                   std::unordered_set<const Composition*>& visited) {
  if (binding == nullptr || scene == nullptr) {
    return;
  }
  for (auto* layer : layers) {
    if (layer == nullptr) {
      continue;
    }
    auto layerRuntime = binding->get<tgfx::Layer>(layer);
    if (layerRuntime == nullptr) {
      layerRuntime = LayerBuilder::BuildLayerInto(layer, binding);
    }
    if (layer->composition != nullptr) {
      auto childComposition = PAGComposition::MakeChild(layer, scene, visited);
      if (childComposition == nullptr) {
        continue;
      }
      if (childComposition->runtimeLayer != nullptr && layerRuntime != nullptr) {
        layerRuntime->addChild(childComposition->runtimeLayer);
      }
      outChildren.push_back(std::move(childComposition));
    } else {
      auto child = std::shared_ptr<PAGLayer>(new PAGLayer(layer, layerRuntime, scene));
      if (!layer->children.empty()) {
        BuildChildren(binding, layer->children, child->children, scene, visited);
        for (auto& nestedChild : child->children) {
          auto nestedSlot = binding->get<tgfx::Layer>(nestedChild->node);
          if (nestedSlot != nullptr && child->runtimeLayer != nullptr) {
            child->runtimeLayer->addChild(nestedSlot);
          }
        }
      }
      outChildren.push_back(std::move(child));
    }
  }
}

void PAGComposition::refreshNodes(const std::vector<Node*>& dirtyNodes,
                                  std::unordered_set<const Composition*>& visited) {
  std::unordered_set<const Node*> dirtySet(dirtyNodes.begin(), dirtyNodes.end());
  const std::vector<Layer*>* sourceLayers = nullptr;
  if (node == nullptr) {
    auto scene = rootScene.lock();
    if (scene != nullptr && scene->document != nullptr) {
      sourceLayers = &scene->document->layers;
    }
  } else if (node->composition != nullptr) {
    sourceLayers = &node->composition->layers;
  }
  bool containerDirty = node == nullptr || dirtySet.find(node) != dirtySet.end();
  if (sourceLayers != nullptr && containerDirty) {
    syncChildren(*sourceLayers, visited);
  }

  if (binding != nullptr) {
    for (auto* dirty : dirtyNodes) {
      if (dirty == nullptr || dirty->nodeType() != NodeType::Layer) {
        continue;
      }
      auto* dirtyLayer = static_cast<Layer*>(dirty);
      if (binding->get<tgfx::Layer>(dirtyLayer) != nullptr) {
        LayerBuilder::RefreshLayerInPlace(dirtyLayer, binding.get());
      }
    }
    // RefreshLayerInPlace's promoteToVectorLayer already swaps the old tgfx instance with the
    // new one via parent->replaceChild. Sync runtimeLayer and layerRegistry so the PAGLayer
    // handle points to the correct instance and hit-test resolves correctly.
    for (auto& child : children) {
      if (child != nullptr && child->node != nullptr && child->layerType() == LayerType::Layer) {
        auto refreshed = binding->get<tgfx::Layer>(child->node);
        if (refreshed != nullptr && refreshed != child->runtimeLayer) {
          auto scene = rootScene.lock();
          if (scene != nullptr) {
            scene->layerRegistry.erase(child->runtimeLayer.get());
            scene->layerRegistry[refreshed.get()] = child.get();
          }
          child->runtimeLayer = refreshed;
        }
      }
    }
  }
  for (auto& child : children) {
    if (child == nullptr) {
      continue;
    }
    if (child->layerType() != LayerType::Layer) {
      auto* childComposition = static_cast<PAGComposition*>(child.get());
      const Composition* childSource =
          childComposition->node != nullptr ? childComposition->node->composition : nullptr;
      bool inserted = false;
      if (childSource != nullptr) {
        inserted = visited.insert(childSource).second;
      }
      childComposition->refreshNodes(dirtyNodes, visited);
      if (inserted) {
        visited.erase(childSource);
      }
    } else if (!child->children.empty()) {
      refreshPlainContainerChildren(child.get(), dirtyNodes, visited, dirtySet);
    }
  }
}

std::shared_ptr<PAGLayer> PAGComposition::BuildChildLayer(
    const Layer* layer, RuntimeBinding* binding, const std::shared_ptr<PAGScene>& scene,
    std::unordered_set<const Composition*>& visited) {
  if (layer->composition != nullptr) {
    auto childComposition = PAGComposition::MakeChild(layer, scene, visited);
    if (childComposition == nullptr) {
      return nullptr;
    }
    auto slot = binding->get<tgfx::Layer>(layer);
    if (slot == nullptr) {
      slot = LayerBuilder::BuildLayerInto(layer, binding);
    }
    if (slot != nullptr && childComposition->runtimeLayer != nullptr) {
      slot->addChild(childComposition->runtimeLayer);
    }
    return childComposition;
  }
  auto layerRuntime = LayerBuilder::BuildLayerInto(layer, binding);
  if (layerRuntime == nullptr) {
    return nullptr;
  }
  auto child = std::shared_ptr<PAGLayer>(new PAGLayer(layer, layerRuntime, scene));
  if (!layer->children.empty()) {
    BuildChildren(binding, layer->children, child->children, scene, visited);
    for (auto& nestedChild : child->children) {
      auto nestedSlot = binding->get<tgfx::Layer>(nestedChild->node);
      if (nestedSlot != nullptr && child->runtimeLayer != nullptr) {
        child->runtimeLayer->addChild(nestedSlot);
      }
    }
  }
  return child;
}

void PAGComposition::syncChildren(const std::vector<Layer*>& sourceLayers,
                                  std::unordered_set<const Composition*>& visited) {
  auto scene = rootScene.lock();
  if (!scene || binding == nullptr || runtimeLayer == nullptr) {
    return;
  }
  std::unordered_map<const Layer*, std::shared_ptr<PAGLayer>> existing = {};
  for (auto& child : children) {
    if (child != nullptr && child->node != nullptr) {
      existing.emplace(child->node, child);
    }
  }
  std::unordered_set<const Layer*> kept = {};
  std::vector<std::shared_ptr<PAGLayer>> newChildren = {};
  newChildren.reserve(sourceLayers.size());
  for (auto* layer : sourceLayers) {
    if (layer == nullptr) {
      continue;
    }
    auto it = existing.find(layer);
    if (it != existing.end()) {
      newChildren.push_back(it->second);
      kept.insert(layer);
      continue;
    }
    auto child = BuildChildLayer(layer, binding.get(), scene, visited);
    if (child != nullptr) {
      newChildren.push_back(std::move(child));
    }
  }
  for (auto& child : children) {
    // Detach the slot (binding entry) rather than child->runtimeLayer: for a composition child
    // the latter is the subtree root nested under the slot, so detaching the slot removes the
    // entire subtree together. The slot must be looked up before binding->remove() erases the
    // mapping.
    if (child == nullptr || child->node == nullptr || kept.find(child->node) != kept.end()) {
      continue;
    }
    auto slot = binding->get<tgfx::Layer>(child->node);
    if (slot != nullptr) {
      slot->removeFromParent();
    }
    binding->remove(child->node);
  }
  children = std::move(newChildren);
  // Reorder this parent's direct tgfx children to match the document order. addChild on a layer
  // already parented here moves it to the top, so appending in source order yields document order.
  for (auto& child : children) {
    if (child == nullptr || child->node == nullptr) {
      continue;
    }
    auto slot = binding->get<tgfx::Layer>(child->node);
    if (slot != nullptr) {
      runtimeLayer->addChild(slot);
    }
  }
}

void PAGComposition::refreshPlainContainerChildren(
    PAGLayer* container, const std::vector<Node*>& dirtyNodes,
    std::unordered_set<const Composition*>& visited,
    const std::unordered_set<const Node*>& dirtySet) {
  if (container == nullptr || container->node == nullptr) {
    return;
  }
  bool dirty = dirtySet.find(container->node) != dirtySet.end();
  if (dirty) {
    auto scene = rootScene.lock();
    if (scene != nullptr && binding != nullptr) {
      // Incrementally sync the container's runtime children with its source layer list, reusing
      // existing PAGLayer / PAGComposition handles so external holders are not invalidated.
      std::unordered_map<const Layer*, std::shared_ptr<PAGLayer>> existing = {};
      for (auto& child : container->children) {
        if (child != nullptr && child->node != nullptr) {
          existing.emplace(child->node, child);
        }
      }
      std::unordered_set<const Layer*> kept = {};
      std::vector<std::shared_ptr<PAGLayer>> newChildren = {};
      newChildren.reserve(container->node->children.size());
      for (auto* sourceChild : container->node->children) {
        if (sourceChild == nullptr) {
          continue;
        }
        auto it = existing.find(sourceChild);
        if (it != existing.end()) {
          newChildren.push_back(it->second);
          kept.insert(sourceChild);
          continue;
        }
        auto child = BuildChildLayer(sourceChild, binding.get(), scene, visited);
        if (child != nullptr) {
          newChildren.push_back(std::move(child));
        }
      }
      for (auto& oldChild : container->children) {
        if (oldChild == nullptr || oldChild->node == nullptr ||
            kept.find(oldChild->node) != kept.end()) {
          continue;
        }
        auto slot = binding->get<tgfx::Layer>(oldChild->node);
        if (slot != nullptr) {
          slot->removeFromParent();
        }
        binding->remove(oldChild->node);
      }
      container->children = std::move(newChildren);
      if (container->runtimeLayer != nullptr) {
        for (auto& child : container->children) {
          if (child == nullptr || child->node == nullptr) {
            continue;
          }
          auto slot = binding->get<tgfx::Layer>(child->node);
          if (slot != nullptr) {
            container->runtimeLayer->addChild(slot);
          }
        }
      }
    }
  }
  // Sync runtimeLayer for plain children that may have been promoted (gained contents →
  // upgraded to VectorLayer) during RefreshLayerInPlace. Runs regardless of whether the
  // container itself is dirty, because a child deeper in the tree may have been promoted.
  for (auto& child : container->children) {
    if (child != nullptr && child->node != nullptr && child->layerType() == LayerType::Layer) {
      auto refreshed = binding->get<tgfx::Layer>(child->node);
      if (refreshed != nullptr && refreshed != child->runtimeLayer) {
        auto sceneSync = rootScene.lock();
        if (sceneSync != nullptr) {
          sceneSync->layerRegistry.erase(child->runtimeLayer.get());
          sceneSync->layerRegistry[refreshed.get()] = child.get();
        }
        child->runtimeLayer = refreshed;
      }
    }
  }
  for (auto& child : container->children) {
    if (child == nullptr) {
      continue;
    }
    if (child->layerType() != LayerType::Layer) {
      static_cast<PAGComposition*>(child.get())->refreshNodes(dirtyNodes, visited);
    } else if (!child->children.empty()) {
      refreshPlainContainerChildren(child.get(), dirtyNodes, visited, dirtySet);
    }
  }
}

}  // namespace pagx
