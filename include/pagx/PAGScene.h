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

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "pagx/PAGAnimation.h"
#include "pagx/PAGComposition.h"
#include "pagx/PAGDisplayOptions.h"
#include "pagx/PAGStateMachine.h"
#include "pagx/PAGTimeline.h"

namespace tgfx {
class Context;
class DisplayList;
class Layer;
class Recording;
}  // namespace tgfx

namespace pagx {

class Animation;
class Node;
class PAGSurface;
class PAGViewModel;
class PAGXDocument;
class SuppressDelegation;
class PAGViewModelValue;
class TextHolder;
class ViewModel;
class Image;
struct Matrix;
struct RuntimeBinding;

/**
 * PAGScene is the runtime host for a PAGXDocument. It builds the runtime layer tree from the
 * document, drives its animations, and draws its content to a surface. Multiple PAGScene instances
 * can be created from the same PAGXDocument; each one has independent runtime state but shares the
 * underlying template data.
 *
 * PAGScene keeps the source PAGXDocument alive internally, so callers may release their own
 * document handle once the PAGScene is created.
 *
 * Thread safety: PAGScene and the PAGAnimation / PAGStateMachine instances it vends are not
 * thread-safe. Their
 * playback and render state carry no internal locking, so all calls on a single PAGScene (and its
 * timelines) must be serialized by the caller. Distinct PAGScene instances built from the same
 * PAGXDocument hold independent runtime state and may be driven from different threads, provided
 * the shared document is not mutated concurrently.
 */
class PAGScene : public std::enable_shared_from_this<PAGScene> {
 public:
  /**
   * Creates a PAGScene bound to the given PAGXDocument. The document is retained for the lifetime
   * of the returned PAGScene, and the PAGScene is registered with the document so that future
   * notifyChange() calls can reach it.
   * @param document the source document. Must be non-null. Make() applies layout automatically if
   * it has not already been applied.
   * @return a shared_ptr to the new PAGScene, or nullptr if the document is null.
   */
  static std::shared_ptr<PAGScene> Make(std::shared_ptr<PAGXDocument> document);

  ~PAGScene();

  /**
   * Returns the ids of all top-level animations in the source document, preserving declaration
   * order. Animations declared inside child Compositions are not included.
   */
  std::vector<std::string> getAnimationIds() const;

  /**
   * Returns the PAGAnimation driving the named top-level animation. Repeated calls with the same
   * id return the same instance, so playback state is shared across all callers driving that
   * animation.
   * @param id an animation id from getAnimationIds(). Returns nullptr if no top-level
   *           Animation matches the given id.
   */
  std::shared_ptr<PAGAnimation> getAnimation(const std::string& id);

  /**
   * Returns the first top-level playback driver (animation or state machine) in declaration order,
   * or nullptr if the document has none. The returned shared_ptr points to a PAGAnimation or
   * PAGStateMachine; use getAnimation() / getStateMachineTimeline() for typed access.
   */
  std::shared_ptr<PAGTimeline> getDefaultTimeline();

  /**
   * Returns the ids of all top-level state machines in the source document, preserving declaration
   * order.
   */
  std::vector<std::string> getStateMachineIds() const;

  /**
   * Returns the PAGStateMachine driving the named top-level state machine. Repeated calls
   * with the same id return the same instance.
   * @param id a state machine id from getStateMachineIds(). Returns nullptr if no top-level
   *           StateMachine matches the given id.
   */
  std::shared_ptr<PAGStateMachine> getStateMachineTimeline(const std::string& id);

  /**
   * Returns the root PAGComposition of this scene.
   */
  std::shared_ptr<PAGComposition> rootComposition() const;

  /**
   * Returns the root ViewModel for this scene, or nullptr if the document has no ViewModel schema.
   */
  std::shared_ptr<PAGViewModel> viewModel() const;

  /**
   * Renders the current content of this scene into the given surface. Does not advance animations;
   * callers must advance the scene (or its timelines) beforehand.
   * @param surface    the destination surface.
   * @param autoClear  if true (the default), the surface is cleared to transparent before drawing.
   *                   Set to false to overlay the scene content on top of the existing surface
   *                   content.
   * @return true if the surface was redrawn, false if surface is null or there is no content.
   */
  bool draw(const std::shared_ptr<PAGSurface>& surface, bool autoClear = true);

  /**
   * Returns the document canvas width.
   */
  float width() const;

  /**
   * Returns the document canvas height.
   */
  float height() const;

  /**
   * Returns the display options used when drawing this PAGScene. The returned object is owned by
   * this PAGScene and remains valid until the PAGScene is destroyed.
   */
  PAGDisplayOptions* getDisplayOptions() const;

  /**
   * Returns true if the scene's content has changed since the last render, i.e. a redraw would
   * produce different pixels. Hosts driving their own render loop can consult this to skip
   * redundant full renders on idle frames. Returns true when either the underlying display list
   * has changed or a pending TextHolder reshape (driven by a ViewModel data bind or animation
   * text-shaping channel) has not yet been flushed — flush runs inside Record().
   */
  bool hasContentChanged() const;

  /**
   * Convenience method equivalent to advance(deltaMicroseconds) followed by apply(). advance() and
   * apply() drive the animations that play automatically in this scene (including nested
   * compositions). Top-level animations are not driven here; play them explicitly via
   * getAnimation(id)->advanceAndApply(...).
   */
  void advanceAndApply(int64_t deltaMicroseconds);

  /**
   * Returns the layers under the given surface point. The first layer in the array is the top-most
   * under the point, the last is the bottom-most. Returns an empty array if nothing is hit. Hit
   * testing does not require a prior draw(). Accepts surface coordinates (with zoom and content
   * offset applied) instead of the composition's local coordinates.
   * @param surfaceX the x coordinate in surface (device) space.
   * @param surfaceY the y coordinate in surface (device) space.
   */
  std::vector<std::shared_ptr<PAGLayer>> getLayersUnderPoint(float surfaceX, float surfaceY);

  /**
   * Returns the displaying bounds of the given layer in surface coordinates, with the layer's full
   * on-screen transform (including animation and the display zoom/offset) applied. Returns an empty
   * rectangle if the layer is null or does not belong to this scene. For the layer's untransformed
   * local bounds, use PAGLayer::getBounds.
   * @param pagLayer a layer handle obtained from this scene (e.g. via getLayersUnderPoint).
   */
  Rect getGlobalBounds(const std::shared_ptr<PAGLayer>& pagLayer) const;

 private:
  PAGScene();

  // Dispatch target for PAGXDocument::notifyChange. Refreshes the runtime tree in place for edits to
  // this document's own nodes; rebuilds the whole tree when the edit comes from an embedded external
  // document (its nodes are not owned by this scene's document).
  void onNodesChanged(const std::vector<Node*>& dirtyNodes);

  // Dispatch target for PAGXDocument::notifyChange when <Image> resource nodes change (e.g. host
  // loadFileData). Re-decodes any ViewModel image value built from a changed Image node, unless the
  // business side has overridden it. Keyed off the Image nodes themselves because a ViewModel
  // schema is not reachable from the Layer tree that onNodesChanged refreshes.
  void onImageResourcesChanged(const std::vector<Image*>& changedImages);
  static void RefreshViewModelImages(PAGComposition* comp,
                                     const std::unordered_set<const Image*>& changed);

  // Builds or rebuilds the runtime layer tree and binding from the document, detaching any previous
  // tree first. Used at creation and when an embedded external document changes (an external
  // composition is built into the tree once and cannot be patched in place).
  void buildRuntimeTree();
  void buildViewModels();
  void buildNestedViewModels(PAGComposition* parentComp);
  static std::shared_ptr<PAGViewModel> CreateViewModelFromSchema(
      ViewModel* schema, const std::shared_ptr<PAGScene>& scene,
      std::unordered_set<const ViewModel*>& visited);
  void flushDataBinds();
  void flushTextHolders();
  // Rebuilds the flat `textHolders` list from the current runtime tree. Called at the end of
  // buildRuntimeTree and after refreshNodes so the list stays in sync with the binding state.
  void collectTextHolders();
  void clearAllViewModelsDirty();
  static void ClearCompositionTreeDirty(PAGComposition* comp);

  RuntimeBinding* mutableBinding();

  tgfx::DisplayList* getDisplayListForOptions() const;

  // Renders the scene into the surface without locking or managing the GPU context. The caller
  // must hold a valid context lock. Internal method used by draw() and pagx::Record().
  bool renderTo(const std::shared_ptr<PAGSurface>& surface, tgfx::Context* context,
                bool autoClear);

  // Converts a surface-space point to the layer tree's root coordinate space using the display
  // list's zoomScale and contentOffset. Returns false if the surface point cannot be mapped (zoom
  // scale is zero). Used by PAGLayer::hitTestPoint so handles can hit-test in surface coordinates.
  bool surfaceToRoot(float surfaceX, float surfaceY, float* rootX, float* rootY) const;

  // Writes the root-to-surface transform (zoomScale then contentOffset) into out and returns true.
  // Returns false if out is null. Used by PAGLayer::getGlobalMatrix to build the local-to-surface
  // matrix. Returned as a pagx Matrix to keep tgfx out of this header.
  bool rootToSurfaceMatrix(Matrix* out) const;

  std::shared_ptr<PAGXDocument> document = nullptr;
  std::shared_ptr<PAGComposition> _rootComposition = nullptr;
  std::shared_ptr<PAGViewModel> rootViewModel = nullptr;
  // Top-level timelines instantiated on demand by getAnimation/getStateMachineTimeline, keyed by
  // their source definition node so repeated lookups return the same instance.
  std::unordered_map<const Node*, std::shared_ptr<PAGTimeline>> instantiatedTimelines = {};

  std::unique_ptr<tgfx::DisplayList> displayList;

  std::unique_ptr<PAGDisplayOptions> displayOptions = nullptr;

  bool suppressNotify = false;
  std::vector<PAGViewModelValue*> pendingNotifications = {};

  // Flat list of every TextHolder registered across the runtime composition tree. Rebuilt at the
  // end of buildRuntimeTree and after refreshNodes (which can replace a binding's holders via
  // RefreshLayerInPlace). flushTextHolders and hasContentChanged iterate this list directly so the
  // common case (no text reshape in the document) avoids walking the composition tree every frame.
  std::vector<std::shared_ptr<TextHolder>> textHolders = {};

  // Maps tgfx layers in the runtime tree to their PAGLayer nodes for hit-test resolution.
  std::unordered_map<const tgfx::Layer*, PAGLayer*> layerRegistry = {};

  friend class PAGXDocument;
  friend class PAGTimeline;
  friend class PAGComposition;
  friend class PAGDisplayOptions;
  friend class PAGLayer;
  friend class PAGViewModelValue;
  friend class SuppressDelegation;
  friend std::unique_ptr<tgfx::Recording> Record(tgfx::Context* context,
                                                  const std::shared_ptr<PAGScene>& scene,
                                                  const std::shared_ptr<PAGSurface>& surface,
                                                  bool autoClear);
};

}  // namespace pagx
