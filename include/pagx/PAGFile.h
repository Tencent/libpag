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
#include <vector>
#include "pagx/PAGComposition.h"
#include "pagx/PAGDisplayOptions.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGTimeline.h"
#include "pagx/PAGXDocument.h"

namespace pagx {

class Animation;
class Node;
struct Matrix;
struct RuntimeBinding;

/**
 * PAGFile is the runtime instance of a PAGXDocument, used to drive its animations and draw its
 * content to a surface. Multiple PAGFile instances can be created from the same PAGXDocument; each
 * one has independent runtime state but shares the underlying template data.
 *
 * PAGFile is a PAGComposition, so the whole file is advanced, applied, and hit-tested through that
 * interface. PAGFile keeps the source PAGXDocument alive internally, so callers may release their
 * own document handle once the PAGFile is created.
 */
class PAGFile : public PAGComposition, public std::enable_shared_from_this<PAGFile> {
 public:
  /**
   * Creates a PAGFile bound to the given PAGXDocument. The document is retained for the lifetime
   * of the returned PAGFile, and the PAGFile is registered with the document so that future
   * notifyChange() calls can reach it.
   * @param document the source document. Must be non-null and have applyLayout() called.
   * @return a shared_ptr to the new PAGFile, or nullptr if the document is null.
   */
  static std::shared_ptr<PAGFile> Make(std::shared_ptr<PAGXDocument> document);

  ~PAGFile() override;

  /**
   * Returns the runtime layer type of the file root, which is always LayerType::File.
   */
  LayerType layerType() const override;

  /**
   * Returns the ids of all top-level animations in the source document, preserving declaration
   * order. Animations declared inside child Compositions are not included.
   */
  std::vector<std::string> getTimelineIds() const;

  /**
   * Returns the PAGTimeline driving the named top-level animation. Repeated calls with the same
   * id return the same instance, so playback state is shared across all callers driving that
   * animation.
   * @param id an animation id from getTimelineIds(). Returns nullptr if no top-level
   *           Animation matches the given id.
   */
  std::shared_ptr<PAGTimeline> getTimeline(const std::string& id);

  /**
   * Returns the PAGTimeline for the first top-level animation in the document, or nullptr if the
   * document has no top-level animations.
   */
  std::shared_ptr<PAGTimeline> getDefaultTimeline();

  /**
   * Renders the current content of this file into the given surface. Does not advance animations;
   * callers must advance the file (or its timelines) beforehand.
   * @param surface the destination surface.
   * @return true if the surface was redrawn, false if surface is null or there is no content.
   */
  bool draw(const std::shared_ptr<PAGSurface>& surface);

  /**
   * Returns the document canvas width.
   */
  float width() const;

  /**
   * Returns the document canvas height.
   */
  float height() const;

  /**
   * Returns the display options used when drawing this PAGFile. The returned object is owned by
   * this PAGFile and remains valid until the PAGFile is destroyed.
   */
  PAGDisplayOptions* getDisplayOptions() const;

  /**
   * Convenience method equivalent to advance(deltaMicroseconds) followed by apply(). advance() and
   * apply() drive the animations that play automatically in this file (including nested
   * compositions). Top-level animations are not driven here; play them explicitly via
   * getTimeline(id)->advanceAndApply(...).
   */
  void advanceAndApply(int64_t deltaMicroseconds);

  /**
   * Returns the layers under the given surface point. The first layer in the array is the top-most
   * under the point, the last is the bottom-most. Returns an empty array if nothing is hit. Hit
   * testing does not require a prior draw(). Overrides PAGComposition to accept surface coordinates
   * (with zoom and content offset applied) instead of the composition's local coordinates.
   * @param surfaceX the x coordinate in surface (device) space.
   * @param surfaceY the y coordinate in surface (device) space.
   */
  std::vector<std::shared_ptr<PAGLayer>> getLayersUnderPoint(float surfaceX,
                                                              float surfaceY) override;

  /**
   * Returns the displaying bounds of the given layer in surface coordinates, with the layer's full
   * on-screen transform (including animation and the display zoom/offset) applied. Returns an empty
   * rectangle if the layer is null or does not belong to this file. For the layer's untransformed
   * local bounds, use PAGLayer::getBounds.
   * @param pagLayer a layer handle obtained from this file (e.g. via getLayersUnderPoint).
   */
  Rect getGlobalBounds(const std::shared_ptr<PAGLayer>& pagLayer) const;

 private:
  PAGFile();

  // PAGXDocument::notifyChange dispatches here.
  void onNodesChanged(const std::vector<Node*>& dirtyNodes);

  RuntimeBinding* mutableBinding();

  void* getDisplayListForOptions() const;

  // Converts a surface-space point to the layer tree's root coordinate space using the display
  // list's zoomScale and contentOffset. Returns false if the surface point cannot be mapped (zoom
  // scale is zero). Used by PAGLayer::hitTestPoint so handles can hit-test in surface coordinates.
  bool surfaceToRoot(float surfaceX, float surfaceY, float* rootX, float* rootY) const;

  // Writes the root-to-surface transform (zoomScale then contentOffset) into out and returns true.
  // Returns false if the transform is unavailable. Used by PAGLayer::getGlobalMatrix to build the
  // local-to-surface matrix. Returned as a pagx Matrix to keep tgfx out of this header.
  bool rootToSurfaceMatrix(Matrix* out) const;

  // Returns the top tgfx layer of the runtime tree as an opaque pointer (the base composition's
  // root). Used by PAGLayer to compute a layer's transform relative to the tree root without
  // requiring the tree to be attached to a display list. Concrete type lives in PAGFile.cpp.
  void* rootRuntimeLayer() const;

  std::shared_ptr<PAGXDocument> document = nullptr;
  std::unordered_map<Animation*, std::shared_ptr<PAGTimeline>> timelinesByAnimation = {};

  // File-level runtime extras not held by the PAGComposition base: the draw surface display list
  // and its attachment flag. The top-level subtree root and binding live in the base composition.
  // The concrete type lives in PAGFile.cpp to avoid pulling tgfx layer types into this header.
  struct FileStorage;
  std::unique_ptr<FileStorage> fileStorage = {};

  std::unique_ptr<PAGDisplayOptions> displayOptions = nullptr;

  friend class PAGXDocument;
  friend class PAGTimeline;
  friend class PAGComposition;
  friend class PAGDisplayOptions;
  friend class PAGLayer;
};

}  // namespace pagx
