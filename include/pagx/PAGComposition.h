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
#include <set>
#include <string>
#include <unordered_set>
#include <vector>
#include "pagx/PAGLayer.h"

namespace pagx {

class PAGAnimation;
class PAGStateMachine;
class PAGTimeline;
class PAGScene;
class PAGViewModel;
class PAGXDocument;
class Composition;
class DataBindRuntime;
class DataContext;
class FontConfig;
class Node;
struct RuntimeBinding;

/**
 * PAGComposition is the runtime instance of a composition. It derives from PAGLayer, adding the
 * ability to drive the animations declared inside the composition. When the same composition is
 * referenced from multiple places, each reference produces an independent PAGComposition with its
 * own playback state, so animating or rendering one does not affect the others.
 */
class PAGComposition : public PAGLayer {
 public:
  ~PAGComposition() override;

  LayerType layerType() const override;

  void advance(int64_t deltaMicroseconds) override;
  void apply(float mix = 1.0f) override;

  /**
   * Returns the ViewModel instance for this composition, or nullptr if the composition has no
   * ViewModel schema.
   */
  std::shared_ptr<PAGViewModel> viewModel() const;

  /**
   * Resumes advancing the named timeline (animation or state machine). The timeline continues to
   * apply() each frame regardless of this state.
   */
  void playTimeline(const std::string& id);

  /**
   * Stops advancing the named timeline. apply() still runs, so the timeline's current frame
   * continues to be written to the content. Call playTimeline() to resume advancing.
   */
  void pauseTimeline(const std::string& id);

  /**
   * Returns true if the named timeline is advancing (not paused).
   */
  bool isTimelinePlaying(const std::string& id) const;

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
  // source layer (PAGComposition for layers that reference a composition, plain PAGLayer for
  // others), associating each node's runtimeLayer with this composition's tgfx layer for that
  // source layer. Composition children's tgfx roots are attached into their slot containers. Must
  // be called after the subtree and binding are built. visited carries the ancestor composition
  // path for cycle detection and is threaded into nested MakeChild calls.
  void buildChildren(const std::vector<Layer*>& layers,
                     std::unordered_set<const Composition*>& visited);

 private:
  // Builds a new runtime PAGLayer or PAGComposition for the given source layer, attaching its
  // tgfx subtree into the parent binding's slots. Shared by syncChildren and
  // refreshPlainContainerChildren so the construction logic stays in one place.
  static std::shared_ptr<PAGLayer> BuildChildLayer(
      const Layer* layer, RuntimeBinding* binding, const std::shared_ptr<PAGScene>& scene,
      std::unordered_set<const Composition*>& visited);

  // Builds the persistent per-layer runtime node tree into outChildren. Internal static helper used
  // by BuildChildLayer and buildChildren.
  static void BuildChildren(RuntimeBinding* binding, const std::vector<Layer*>& layers,
                            std::vector<std::shared_ptr<PAGLayer>>& outChildren,
                            const std::shared_ptr<PAGScene>& scene,
                            std::unordered_set<const Composition*>& visited);

  // Collects the direct child compositions of a layer into outChildren, transparently descending
  // through plain PAGLayer containers (which may nest compositions) but not into the child
  // compositions themselves. Used by the composition-tree walks (view-model build, data-bind
  // update, view-model advance) so that compositions nested under plain container layers are not
  // skipped.
  static void CollectChildCompositions(PAGLayer* layer, std::vector<PAGComposition*>& outChildren);

  // Refreshes this composition after edits: reconciles its child layer list and refreshes any dirty
  // leaf layers in place, then recurses into child compositions. Called by PAGScene::onNodesChanged.
  // visited carries the source compositions on the current ancestor path: when this method recurses
  // into a child composition, the child's own source is inserted before recursing and erased on
  // return, so any newly added layer that references an ancestor composition is detected at the top
  // of MakeChild rather than one frame deeper.
  void refreshNodes(const std::vector<Node*>& dirtyNodes,
                    const std::unordered_set<const Node*>& dirtySet,
                    std::unordered_set<const Composition*>& visited);

  // Reconciles this composition's runtime children with the given source layer list: reuses
  // children whose source layer still maps to a tgfx layer (handles stay valid), builds newly added
  // layers into the binding, removes runtime children whose source layer is gone, and reorders the
  // parent's tgfx children to match the document order. visited is the ancestor path of source
  // compositions; the caller must include this composition's own source so MakeChild rejects a
  // newly added layer that points back to this composition or any ancestor at the top of the call.
  void syncChildren(const std::vector<Layer*>& sourceLayers,
                    std::unordered_set<const Composition*>& visited);

  // Rebuilds this composition's timelines from the owner layer's animation drivers, discarding any
  // existing ones first so removed drivers or animations simply stop driving. Used at build time and
  // when a timeline node changes. The root composition has no owner layer and spawns no timelines.
  void spawnTimelines(const std::shared_ptr<PAGScene>& scene);

  // Spawns timelines for this composition using its own scene reference. Internal helper.
  void spawnTimelinesFromScene();

  // Static visitor for PAGLayer::forEachComposition. Calls spawnTimelinesFromScene on each
  // PAGComposition in the tree.
  static void ResetCompositionTimelines(PAGComposition* comp);

  // Resets the timelines of this composition and all descendant compositions. Called when an edit
  // touches a timeline node, rebuilding the whole timeline tree rather than patching it in place.
  void resetTimelines();

  // Override to include this PAGComposition node itself in the traversal after visiting children.
  void forEachComposition(void (*visitor)(PAGComposition*)) override;

  // Rebuilds children of a plain PAGLayer container whose source node is dirty, and recurses into
  // its descendant plain containers. Called by refreshNodes.
  void refreshPlainContainerChildren(PAGLayer* container,
                                     const std::vector<Node*>& dirtyNodes,
                                     std::unordered_set<const Composition*>& visited,
                                     const std::unordered_set<const Node*>& dirtySet);

  // Document used to resolve channel target IDs for timelines spawned by this composition. For a
  // sealed external composition this is the layer's externalDoc; otherwise the scene's document.
  PAGXDocument* document = nullptr;
  std::unique_ptr<RuntimeBinding> binding;
  std::vector<std::shared_ptr<PAGTimeline>> timelines = {};
  // Timeline ids that are paused. advance() skips these, but apply() still runs so the current
  // frame keeps contributing to the output. Mirrors Rive's NestedSimpleAnimation.isPlaying, which
  // gates time advancement but not application.
  std::set<std::string> pausedTimelineIds = {};
  std::shared_ptr<PAGViewModel> compositionViewModel = nullptr;
  std::unique_ptr<DataBindRuntime> dataBindRuntime;
  std::shared_ptr<DataContext> dataContext = nullptr;

  void updateDataBinds(float mix = 1.0f);

  // Flushes every TextHolder in this composition's binding and its child compositions, reshaping
  // ViewModel/Animation-driven text once per draw. fontConfig is borrowed for the call.
  void flushTextHolders(FontConfig* fontConfig);

  friend class PAGScene;
};

}  // namespace pagx
