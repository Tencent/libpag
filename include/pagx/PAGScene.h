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
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "pagx/PAGAnimation.h"
#include "pagx/PAGComposition.h"
#include "pagx/PAGDisplayOptions.h"
#include "pagx/PAGOutputViewStatus.h"
#include "pagx/PAGStateMachine.h"
#include "pagx/PAGTimeline.h"
#include "pagx/types/Point.h"
#include "pagx/types/Rect.h"

namespace tgfx {
class Context;
class DisplayList;
class Recording;
}  // namespace tgfx

namespace pagx {

class Animation;
class Node;
class PAGLayer;
class PAGSurface;
class PAGViewModel;
class PAGXDocument;
class SuppressDelegation;
class PAGViewModelValue;
class TextHolder;
class ViewModel;
class Image;
class OutputTargetState;
struct Matrix;
struct OutputViewValidationSnapshot;
struct RuntimeBinding;

/**
 * Snapshot of a scene's output view state, returned by
 * PAGScene::getOutputViewInfo().
 */
struct OutputViewInfo {
  /**
   * None when the document renders the whole canvas, Valid when draw() renders only the target
   * layer into bounds, Invalid when the declaration cannot be honored.
   */
  OutputViewStatus status = OutputViewStatus::None;

  /**
   * The output view bounds in document-root coordinates when status is Valid, otherwise
   * std::nullopt. It reflects the document's current bounds, so it is std::nullopt when a host
   * cleared outputBounds without notifying the scene.
   */
  std::optional<Rect> bounds = std::nullopt;

  /**
   * Diagnostic for an invalid declaration; empty when status is None or Valid.
   */
  std::string error = {};
};

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
   * @return a shared_ptr to the new PAGScene, or nullptr if the document is null or declares an
   * invalid output view (the paired `targetLayer` / `outputBounds` attributes).
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
   * callers must advance the scene (or its timelines) beforehand. When the document declares a
   * default output view, only the target layer is rendered into the outputBounds rectangle. Use
   * getOutputBounds() together with the effective display zoom and content offset to size and
   * position the output on the surface.
   * This method manages the surface device's context lock internally. Callers must not already
   * hold that lock; use pagx::Record() from pagx/tgfx.h to render with an explicitly locked context.
   * @param surface    the destination surface.
   * @param autoClear  if true (the default), the surface is cleared to transparent before drawing.
   *                   Set to false to overlay the scene content on top of the existing surface
   *                   content.
   * @return true if the surface was redrawn, false if surface is null, there is no content, or the
   * document's output view declaration is invalid (see getOutputViewStatus()).
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
   * Returns the output rectangle, in document-root coordinates, that draw() renders when the
   * document declares a valid output view (the paired `targetLayer` / `outputBounds`
   * root attributes). The scene subtracts this window's origin exactly once; hosts use its width
   * and height for fitting and must not subtract the origin again in their content offset.
   * After setting zoom and precision, read getDisplayOptions()->getZoomScale() for the effective
   * drawing scale, which may differ from the requested value because of quantization. When that
   * scale is positive and the content offset is zero, the natural output pixel size is
   * ceil(bounds.width * effectiveScale) by ceil(bounds.height * effectiveScale). A non-positive
   * scale cannot produce a drawable output. For a panned or centered viewport, also account for
   * getDisplayOptions()->getContentOffset(): surfacePoint = offset + effectiveScale *
   * (rootPoint - bounds.origin). width() and height() still report the original document canvas.
   * draw() clips content to the output view bounds; uncovered surface pixels retain the configured
   * background when clearing, so they are transparent only when that background is transparent.
   * @return the output rectangle, or std::nullopt when the document has no output view and draw()
   *         renders the whole canvas, or when the declaration is invalid. Use getOutputViewInfo()
   *         to read the status, bounds and diagnostic in one call.
   */
  std::optional<Rect> getOutputBounds() const;

  /**
   * Returns the scene's output view state in a single query. The status tells None
   * from Valid and Invalid, bounds is set only when the view is valid, and error is set only when
   * it is invalid.
   */
  OutputViewInfo getOutputViewInfo() const;

  /**
   * Returns the state of the document's output view. None means the document renders
   * the whole canvas; Valid means draw() renders only the target layer into getOutputBounds();
   * Invalid means the declaration cannot be honored and draw() fails until the declaration is fixed
   * or cleared. The state is refreshed when the document notifies the scene of a change.
   */
  OutputViewStatus getOutputViewStatus() const;

  /**
   * Returns a copy of the diagnostic for an invalid output view, or an empty string
   * when the view is absent or valid. The copy stays valid after the scene changes.
   */
  std::string getOutputViewError() const;

  /**
   * Returns the display options used when drawing this PAGScene. The returned object is owned by
   * this PAGScene and remains valid until the PAGScene is destroyed. When the document renders a
   * output view, only the zoom scale, content offset and background color affect the
   * output; the render mode, tile options and subtree cache do not, because the target layer is
   * drawn directly instead of through the display list.
   */
  PAGDisplayOptions* getDisplayOptions() const;

  /**
   * Returns true if the scene's content has changed since the last render, i.e. a redraw would
   * produce different pixels. Hosts driving their own render loop can consult this to skip
   * redundant full renders on idle frames. Returns true when either the underlying display list
   * has changed or a pending TextHolder reshape (driven by a ViewModel data bind or animation
   * text-shaping channel) has not yet been flushed — flush runs inside Record(). Static output
   * views track document/resource notifications and effective display transform/background changes
   * independently of the temporary layer-tree changes used while drawing the target.
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
   * When the document declares an output view, only the target layer and its descendants are
   * searched, and the point must map inside the declared output view bounds. A declaration that
   * cannot be honored returns an empty array.
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
  // document (its nodes are not owned by this scene's document). `dirtyNodes` is the collapsed
  // Layer set used for the runtime refresh; `reportedDirty` is the pre-collapse owned set used to
  // decide whether the output view declaration still has to be re-validated.
  void onNodesChanged(const std::vector<Node*>& dirtyNodes, const std::vector<Node*>& reportedDirty,
                      bool layoutChanged);

  // Dispatch target for PAGXDocument::notifyChange when <Image> resource nodes change (e.g. host
  // loadFileData). Re-decodes any ViewModel image value built from a changed Image node, unless the
  // business side has overridden it. Keyed off the Image nodes themselves because a ViewModel
  // schema is not reachable from the Layer tree that onNodesChanged refreshes.
  void onImageResourcesChanged(const std::vector<Image*>& changedImages);
  static void RefreshViewModelImages(PAGComposition* comp,
                                     const std::unordered_set<const Image*>& changed);

  // Builds or rebuilds the runtime layer tree and binding from the document, detaching any previous
  // tree first. Used at creation and when an embedded external document changes (an external
  // composition is built into the tree once and cannot be patched in place). preResolvedOutputView
  // is set when the caller already stored the resolution in the output view fields, so the tree
  // build only captures the validation snapshot instead of resolving the declaration again.
  void buildRuntimeTree(bool preResolvedOutputView);
  // Resolves and caches the document's default output view state and target layer after a tree
  // build or edit. Cached so renderTo() does not walk the authored layer tree on every frame.
  void resolveOutputTarget();
  // Captures the snapshot used by canReuseOutputViewValidation() from the currently cached status
  // and target. Counts as one full resolver run.
  void captureOutputViewValidation();
  // Returns true when the notification cannot change the output view declaration, so the cached
  // Valid status and target are reused instead of running ResolveOutputView() again.
  bool canReuseOutputViewValidation(const std::vector<Node*>& reportedDirty,
                                    bool layoutChanged) const;
  // Lazily prepares and validates the reusable target-view wrapper for this scene. Returns false
  // when no target is bound, a stored preparation no longer matches the binding, or the wrapper
  // could not be prepared. A failed preparation is never left cached as a successful one.
  bool ensureOutputTargetState();
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

  void reapplyDataBindsForTarget(const Node* targetNode, RuntimeBinding* scopeBinding);

  RuntimeBinding* mutableBinding();

  tgfx::DisplayList* getDisplayListForOptions() const;

  // Renders the scene into the surface without locking or managing the GPU context. The caller
  // must hold a valid context lock. Internal method used by draw() and pagx::Record().
  bool renderTo(const std::shared_ptr<PAGSurface>& surface, tgfx::Context* context, bool autoClear);

  // Converts a surface-space point to the layer tree's root coordinate space using the display
  // list's zoomScale and contentOffset, plus the output view bounds origin when an output view is
  // active. Returns false if the surface point cannot be mapped (zoom scale is zero). Used by
  // PAGLayer::hitTestPoint so handles can hit-test in surface coordinates.
  bool surfaceToRoot(float surfaceX, float surfaceY, float* rootX, float* rootY) const;

  // Writes the root-to-surface transform (zoomScale then contentOffset, shifted by the output
  // window origin when an output view is active) into out and returns true. Returns false if out is
  // null. Used by PAGLayer::getGlobalMatrix to build the local-to-surface matrix. Returned as a
  // pagx Matrix to keep tgfx out of this header.
  bool rootToSurfaceMatrix(Matrix* out) const;

  // Returns the root-space origin of the output view bounds that draw() subtracts for an output
  // view, or (0, 0) when the scene renders the whole canvas. Query helpers apply the same shift so
  // bounds, matrices and hit testing stay in the surface space the caller actually sees.
  Point outputViewOrigin() const;

  // Resolves the runtime search root and the root-space point for a hit test. Returns false when
  // the scene cannot answer: an invalid output view, a point outside the declared output view
  // bounds, a zero zoom scale, or a missing runtime tree. A valid output view searches only the
  // target layer, so hits outside the rendered subtree are never reached.
  bool prepareHitTest(float surfaceX, float surfaceY, Point* rootPoint,
                      std::shared_ptr<tgfx::Layer>* hitRoot) const;

  // Hit tests one layer handle against a surface point. Honors the output view bounds and target
  // subtree when the document declares one.
  bool hitTestLayer(const PAGLayer* layer, float surfaceX, float surfaceY, bool pixelHitTest) const;

  std::shared_ptr<PAGXDocument> document = nullptr;
  std::shared_ptr<PAGComposition> _rootComposition = nullptr;
  std::shared_ptr<PAGViewModel> rootViewModel = nullptr;
  // Top-level timelines instantiated on demand by getAnimation/getStateMachineTimeline, keyed by
  // their source definition node so repeated lookups return the same instance.
  std::unordered_map<const Node*, std::shared_ptr<PAGTimeline>> instantiatedTimelines = {};

  std::unique_ptr<tgfx::DisplayList> displayList;

  // Authored layer that the document declares as its default static output target, or nullptr.
  const Layer* outputTargetNode = nullptr;

  // Cached state of the document's default output view. An edit can turn a valid declaration into
  // an invalid one (for example clearing targetLayerId while keeping outputBounds); renderTo() must
  // reject that state instead of falling back to a whole-canvas render.
  OutputViewStatus outputViewStatus = OutputViewStatus::None;
  std::string outputViewError = {};

  // Tracks document, resource and display changes that require a target-view redraw. The reusable
  // wrapper is kept separately in outputTargetState; repreparing it is not itself a content change.
  // Consume this flag only after a successful target draw.
  bool outputViewDirty = true;

  // Snapshot captured by the last successful full validation. Compared against each notification
  // to skip repeated resolver scans for render-only leaf edits.
  std::unique_ptr<OutputViewValidationSnapshot> outputViewValidation = nullptr;

  // Number of full resolver runs and fast-path reuses since construction. Tests read these to
  // assert that a notification took the intended path without relying on machine-dependent timing.
  uint64_t outputViewValidationRuns = 0;
  uint64_t outputViewValidationReuses = 0;

  // Reusable identity wrapper for the output view. Prepared on the first successful draw and
  // released before any tree rebuild, node edit or image refresh. Ordinary targets keep the
  // wrapper across frames; external mask targets restore their hookup after every draw so public
  // aggregate bounds queries keep their original semantics.
  std::unique_ptr<OutputTargetState> outputTargetState = nullptr;

  std::unique_ptr<PAGDisplayOptions> displayOptions = nullptr;

  bool suppressNotify = false;
  std::vector<PAGViewModelValue*> pendingNotifications = {};

  // Flat list of every TextHolder registered across the runtime composition tree. Rebuilt at the
  // end of buildRuntimeTree and after refreshNodes (which can replace a binding's holders via
  // RefreshLayerInPlace). flushTextHolders and hasContentChanged iterate this list directly so the
  // common case (no text reshape in the document) avoids walking the composition tree every frame.
  std::vector<std::shared_ptr<TextHolder>> textHolders = {};

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
