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

std::shared_ptr<PAGComposition> PAGComposition::MakeChild(
    const Layer* ownerLayer, const std::shared_ptr<PAGScene>& parentScene,
    std::unordered_set<const Composition*>& visited) {
  if (ownerLayer == nullptr || parentScene == nullptr || ownerLayer->composition == nullptr) {
    return nullptr;
  }
  // Cycle guard: visited holds the compositions on the current ancestor path. A composition that
  // references itself (directly or via an A->B->A chain) would otherwise recurse without end and
  // overflow the stack. Report the cycle on the owning document and drop this subtree. The set is
  // treated as a path stack (inserted on the way down, erased on return) so a composition reused
  // by sibling layers is not mistaken for a cycle. The external-file chain guard in
  // PAGXDocument::LoadExternalComposition does not cover same-document @id references.
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
  // Register this host scene with the external document so that editing the external (child)
  // document and calling its own notifyChange refreshes this scene's embedded subtree. The child
  // document keeps only a weak reference, so it never keeps the scene alive.
  if (externalDoc != nullptr) {
    externalDoc->registerLiveScene(parentScene);
  }
  // Spawn the timelines declared on the owner layer, targeting this composition's own binding and
  // document, then build the persistent per-layer runtime node tree for the composition content.
  composition->spawnTimelines(parentScene);
  visited.insert(sourceComposition);
  composition->buildChildren(ownerLayer->composition->layers, visited);
  visited.erase(sourceComposition);
  return composition;
}

void PAGComposition::spawnTimelines(const std::shared_ptr<PAGScene>& scene) {
  // Discard any existing timelines first so a removed driver or animation produces no timeline and a
  // removed animation node (findNode returns null below) simply leaves nothing to drive.
  timelines.clear();
  // The root composition (node == nullptr) has no owner layer and therefore no drivers.
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

void PAGComposition::resetTimelines() {
  auto scene = rootScene.lock();
  if (scene != nullptr) {
    spawnTimelines(scene);
  }
  for (auto& child : children) {
    if (child != nullptr && child->layerType() != LayerType::Layer) {
      static_cast<PAGComposition*>(child.get())->resetTimelines();
    }
  }
}

void PAGComposition::buildChildren(const std::vector<Layer*>& layers,
                                   std::unordered_set<const Composition*>& visited) {
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
      auto childComposition = PAGComposition::MakeChild(layer, scene, visited);
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

void PAGComposition::refreshNodes(const std::vector<Node*>& dirtyNodes,
                                  std::unordered_set<const Composition*>& visited) {
  std::unordered_set<const Node*> dirtySet(dirtyNodes.begin(), dirtyNodes.end());
  // Reconcile the child layer list first. A dirty container node means its child list may have
  // gained or lost layers. The root composition (node == nullptr) reconciles against the document's
  // top-level layers; a child composition reconciles against its source composition's layers.
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

  // Refresh the attributes/contents of any dirty layer nodes owned by this composition's binding.
  // Iterate the dirty set rather than the top-level children so nested child layers (Layer.children
  // built into the same binding) are refreshed too. RefreshLayerInPlace also reconciles each dirty
  // layer's own child list, so adding or removing nested children takes effect when the parent layer
  // is marked dirty.
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
    // RefreshLayerInPlace may swap a plain child's tgfx layer instance (a plain layer promoted to a
    // VectorLayer once it gains contents). Re-sync only plain children: for them the binding entry
    // is the runtimeLayer itself. A composition child is skipped because its binding entry is the
    // empty slot while its runtimeLayer is the nested subtree root, so re-syncing from the binding
    // would overwrite runtimeLayer with the slot and break hit-testing, bounds and nested re-attach.
    for (auto& child : children) {
      if (child != nullptr && child->node != nullptr && child->layerType() == LayerType::Layer) {
        auto refreshed = binding->get<tgfx::Layer>(child->node);
        if (refreshed != nullptr && refreshed != child->runtimeLayer) {
          child->runtimeLayer = refreshed;
        }
      }
    }
  }
  // Timelines are intentionally left untouched here: they are reset as a whole tree by PAGScene
  // (resetTimelines) only when a timeline node changed, so a plain attribute or structural edit does
  // not disturb in-progress playback.
  for (auto& child : children) {
    if (child != nullptr && child->layerType() != LayerType::Layer) {
      auto* childComposition = static_cast<PAGComposition*>(child.get());
      // Push the child composition's source onto the ancestor path before recursing, so any layer
      // newly added inside it that references an ancestor (including this composition) is detected
      // at the top of MakeChild. Only erase what this frame inserted: a sibling subtree that
      // legitimately shares the same downstream composition would otherwise drop the marker.
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
    }
  }
}

void PAGComposition::syncChildren(const std::vector<Layer*>& sourceLayers,
                                  std::unordered_set<const Composition*>& visited) {
  auto scene = rootScene.lock();
  if (!scene || binding == nullptr || runtimeLayer == nullptr) {
    return;
  }
  // Index existing runtime children by their source layer node so layers that still exist are
  // reused unchanged (their tgfx layers and handles remain valid).
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
    // Newly added layer: build its tgfx subtree into this binding and wrap it in a runtime node.
    if (layer->composition != nullptr) {
      // Reuse the ancestor path threaded in by refreshNodes so a newly added layer that references
      // an ancestor composition is rejected at the top of MakeChild rather than only after one
      // wasted PAGComposition allocation deeper in the recursion.
      auto childComposition = PAGComposition::MakeChild(layer, scene, visited);
      if (childComposition == nullptr) {
        continue;
      }
      auto slot = binding->get<tgfx::Layer>(layer);
      if (slot == nullptr) {
        slot = LayerBuilder::BuildLayerInto(layer, binding.get());
      }
      if (slot != nullptr && childComposition->runtimeLayer != nullptr) {
        slot->addChild(childComposition->runtimeLayer);
      }
      newChildren.push_back(std::move(childComposition));
    } else {
      auto layerRuntime = LayerBuilder::BuildLayerInto(layer, binding.get());
      if (layerRuntime == nullptr) {
        continue;
      }
      newChildren.push_back(std::shared_ptr<PAGLayer>(new PAGLayer(layer, layerRuntime, scene)));
    }
  }
  // Remove runtime children whose source layer is gone: detach their tgfx layer from this parent
  // and drop their binding entries so no stale mapping survives. Detach the slot (the binding
  // entry, which is this composition's direct child), not child->runtimeLayer: for a composition
  // child the latter is the subtree root nested under the slot, so detaching it would orphan the
  // empty slot container. Removing the slot detaches it together with its nested subtree. The slot
  // must be looked up before binding->remove() erases the mapping.
  for (auto& child : children) {
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
  // Reorder this parent's direct child tgfx layers to match the document order. The direct child of
  // this composition's runtimeLayer is the layer's slot (binding entry), not child->runtimeLayer: a
  // composition child's runtimeLayer is the subtree root nested under its slot. addChild on a layer
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
