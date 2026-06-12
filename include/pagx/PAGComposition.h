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

#pragma once

#include <cstdint>
#include <memory>
#include <unordered_set>
#include <vector>
#include "pagx/PAGLayer.h"

namespace pagx {

class PAGTimeline;
class PAGScene;
class PAGXDocument;
class Composition;
class Node;
struct RuntimeBinding;

/**
 * PAGComposition is the runtime instance of a composition. It derives from PAGLayer, adding the
 * ability to drive the animations declared inside the composition and to answer hit-test queries
 * against its content. When the same composition is referenced from multiple places, each reference
 * produces an independent PAGComposition with its own playback state, so animating or hit-testing
 * one does not affect the others.
 */
class PAGComposition : public PAGLayer {
 public:
  ~PAGComposition() override;

  LayerType layerType() const override;

  /**
   * Advances the animations that play automatically inside this composition, including those in
   * nested child compositions, by the given amount of time.
   * @param deltaMicroseconds the elapsed time in microseconds. May be negative.
   */
  void advance(int64_t deltaMicroseconds);

  /**
   * Applies the current state of the animations that play automatically inside this composition,
   * including those in nested child compositions, to the displayed content.
   * @param mix blend weight, defaults to 1.0 (full overwrite).
   */
  void apply(float mix = 1.0f);

  /**
   * Returns the leaf layers under the given point in this composition's coordinate space.
   * Composition nodes are excluded from results; only concrete renderable layers are returned.
   * The first layer in the array is the top-most under the point, the last is the bottom-most.
   * Returns an empty array if nothing is hit. Hit testing does not require the content to have
   * been drawn first.
   * @param x the x coordinate in this composition's coordinate space.
   * @param y the y coordinate in this composition's coordinate space.
   */
  virtual std::vector<std::shared_ptr<PAGLayer>> getLayersUnderPoint(float x, float y);

 protected:
  // Constructs a runtime composition node bound to the given source layer (null for the root
  // composition), its built subtree root, and the root PAGScene. The subtree, binding, timelines,
  // and children are populated by the PAGComposition/PAGScene factories after construction.
  PAGComposition(const Layer* node, std::shared_ptr<tgfx::Layer> runtimeLayer,
                 const std::shared_ptr<PAGScene>& scene);

  // Builds a runtime child composition for ownerLayer.composition. Returns nullptr if ownerLayer
  // is null, has no composition, or the document is not laid out. visited holds the compositions
  // on the current ancestor path so a self- or mutually-referencing composition is detected and
  // dropped instead of recursing without end. Internal factory used by PAGScene and
  // PAGComposition to populate child runtime nodes.
  static std::shared_ptr<PAGComposition> MakeChild(const Layer* ownerLayer,
                                                   const std::shared_ptr<PAGScene>& parentScene,
                                                   std::unordered_set<const Composition*>& visited);

  // Builds the persistent per-layer runtime node tree for this composition: one PAGLayer node per
  // source layer (PAGComposition for layers that reference a composition, plain PAGLayer otherwise),
  // associating each node's runtimeLayer with this composition's tgfx layer for that source layer.
  // Composition children's tgfx roots are attached into their slot containers. Must be called after
  // the subtree and binding are built. visited carries the ancestor composition path for cycle
  // detection and is threaded into nested MakeChild calls.
  void buildChildren(const std::vector<Layer*>& layers,
                     std::unordered_set<const Composition*>& visited);

  // Resolves a hit tgfx layer to the persistent PAGLayer node whose runtimeLayer matches it,
  // searching this composition's children and recursing into descendant composition children.
  // Returns nullptr if no persistent node owns the layer (internal sub-layer).
  std::shared_ptr<PAGLayer> findChildForLayer(const tgfx::Layer* hitLayer);

  // Refreshes this composition after edits: reconciles its child layer list (adding, removing, and
  // reordering children to match the source layers when the container node is dirty), refreshes the
  // content of any dirty leaf layers in place, then recurses into child compositions so each
  // refreshes only the nodes in its own binding. Timelines are not touched here; PAGScene resets the
  // whole timeline tree separately when a timeline node changed. Called by PAGScene::onNodesChanged.
  void refreshNodes(const std::vector<Node*>& dirtyNodes);

  // Reconciles this composition's runtime children with the given source layer list: reuses
  // children whose source layer still maps to a tgfx layer (handles stay valid), builds newly added
  // layers into the binding, removes runtime children whose source layer is gone, and reorders the
  // parent's tgfx children to match the document order.
  void syncChildren(const std::vector<Layer*>& sourceLayers);

  // Rebuilds this composition's timelines from the owner layer's AnimationTimeline drivers,
  // resolving each driver's animation id against the document. Discards any existing timelines
  // first, so a removed driver or animation simply produces no timeline and a removed animation node
  // (findNode returns null) leaves nothing to drive. Used at build time and to reset timelines when
  // a timeline node changes. The root composition has no owner layer and spawns no timelines.
  void spawnTimelines(const std::shared_ptr<PAGScene>& scene);

  // Recursively resets the timelines of this composition and all descendant compositions. Called
  // when an edit touches a timeline node (Animation / AnimationObject / Channel), since timelines
  // may share targets and cross-reference, so the whole timeline tree is rebuilt rather than patched
  // in place.
  void resetTimelines();

  // Document used to resolve channel target IDs for timelines spawned by this composition. For a
  // sealed external composition this is the layer's externalDoc; otherwise the scene's document.
  PAGXDocument* document = nullptr;
  std::unique_ptr<RuntimeBinding> binding;
  std::vector<std::shared_ptr<PAGTimeline>> timelines = {};
  // The full per-layer runtime node tree of this composition, one entry per source layer, mixing
  // plain PAGLayer leaves and PAGComposition children. Persistent for the PAGScene lifetime.
  std::vector<std::shared_ptr<PAGLayer>> children = {};

  friend class PAGScene;
  friend class PAGTimeline;
};

}  // namespace pagx
