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
#include "pagx/PAGSurface.h"
#include "pagx/PAGTimeline.h"
#include "pagx/PAGXDocument.h"

namespace pagx {

class Animation;
class Node;
class PAGComposition;

/**
 * PAGFile is the runtime instance of a PAGXDocument. It owns the runtime layer tree, the timeline
 * registry, and the draw surface binding. Multiple PAGFile instances can be created from the same
 * PAGXDocument; each one has independent runtime state but shares the underlying template data.
 *
 * PAGFile keeps the source PAGXDocument alive through a shared_ptr held internally, so callers
 * may release their own document handle once the PAGFile is created.
 */
class PAGFile : public std::enable_shared_from_this<PAGFile> {
 public:
  /**
   * Creates a PAGFile bound to the given PAGXDocument. The document is retained for the lifetime
   * of the returned PAGFile, and the PAGFile is registered with the document so that future
   * notifyChange() calls can reach it.
   * @param document the source document. Must be non-null and have applyLayout() called.
   * @return a shared_ptr to the new PAGFile, or nullptr if the document is null.
   */
  static std::shared_ptr<PAGFile> Make(std::shared_ptr<PAGXDocument> document);

  ~PAGFile();

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
   * Returns the PAGTimeline for the first top-level animation, or nullptr if the document has no
   * top-level animations. Equivalent to getTimeline(getTimelineIds().front()).
   */
  std::shared_ptr<PAGTimeline> getDefaultTimeline();

  /**
   * Renders the current state of the runtime tree into the given surface. Does not advance
   * timelines; callers must invoke advance() / apply() on the appropriate timelines beforehand.
   * @param surface the destination surface.
   * @return true if the surface was redrawn, false if surface is null or the runtime tree is
   *         empty.
   */
  bool draw(const std::shared_ptr<PAGSurface>& surface);

  /**
   * Returns the document canvas width.
   */
  float getWidth() const;

  /**
   * Returns the document canvas height.
   */
  float getHeight() const;

  /**
   * Advances the master clock by deltaMicroseconds and applies the result to the runtime tree.
   * The master clock drives the default top-level timeline (animations[0]) and every spawned slot
   * timeline inside Composition slots; non-default top-level timelines are independent and must
   * be advanced via getTimeline(...) directly.
   *
   * Mirrors Rive's artboard.advanceAndApply(dt) idiom: business code typically just calls
   * file->advance(dt) per frame, then file->draw(surface).
   */
  void advance(int64_t deltaMicroseconds);

  /**
   * Re-applies the default timeline plus every spawned slot timeline at their current times,
   * without advancing. Useful after notifyChange or when a new PAGSurface is bound.
   */
  void apply();

  /**
   * Convenience method equivalent to advance(deltaMicroseconds) followed by apply().
   */
  void advanceAndApply(int64_t deltaMicroseconds);

 private:
  PAGFile() = default;

  // PAGXDocument::notifyChange dispatches here.
  void onNodesChanged(const std::vector<Node*>& dirtyNodes);

  // Evaluates the given animation at the given microsecond time and writes results into the
  // supplied per-slot layerTree (or the top-level layerTree when slotLayerTree is null).
  // Called by PAGTimeline::apply().
  void applyAnimation(Animation* animation, PAGLayerTree* slotLayerTree, int64_t microseconds,
                      float mix);

  // Constructs a PAGTimeline targeting the given animation, applying its writes to the supplied
  // per-slot layerTree. Used by PAGComposition::Make for slot-spawned timelines. Caller owns the
  // returned shared_ptr; the file does not register slot timelines into timelinesByAnimation.
  std::shared_ptr<PAGTimeline> createSlotTimeline(Animation* animation, PAGLayerTree* layerTree);

  std::shared_ptr<PAGXDocument> document = nullptr;
  std::unordered_map<Animation*, std::shared_ptr<PAGTimeline>> timelinesByAnimation = {};

  // Composition slots, one per top-level Layer with composition!=null.
  std::vector<std::unique_ptr<PAGComposition>> compositionSlots = {};

  // The runtime layer tree opaque pointer; concrete type lives in PAGFile.cpp to avoid pulling
  // tgfx layer types into the public header.
  struct LayerTreeStorage;
  std::unique_ptr<LayerTreeStorage> layerTree = {};

  friend class PAGXDocument;
  friend class PAGTimeline;
  friend class PAGComposition;
};

}  // namespace pagx
