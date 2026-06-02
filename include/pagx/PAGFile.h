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
struct RuntimeBinding;

/**
 * PAGFile is the runtime instance of a PAGXDocument. It owns the runtime layer tree, the timeline
 * registry, and the draw surface binding. Multiple PAGFile instances can be created from the same
 * PAGXDocument; each one has independent runtime state but shares the underlying template data.
 *
 * PAGFile is the top-level PAGComposition: its base holds the file's top-level runtime subtree,
 * binding, and the nested child compositions. PAGFile adds the file-level state (source document,
 * timeline registry, display options, and the draw surface binding).
 *
 * PAGFile keeps the source PAGXDocument alive through a shared_ptr held internally, so callers
 * may release their own document handle once the PAGFile is created.
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
   * Returns the PAGTimeline for the first top-level animation in the document, or nullptr if the
   * document has no top-level animations.
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
   * Returns the display options used when drawing this PAGFile. The returned object is owned by
   * this PAGFile and remains valid until the PAGFile is destroyed.
   */
  PAGDisplayOptions* getDisplayOptions() const;

  /**
   * Advances every runtime composition (recursing into nested child compositions) by
   * deltaMicroseconds and applies the result to the runtime tree. Drives only the timelines
   * spawned by Layer.timelines drivers that declare auto-play; top-level animations are not
   * touched and must be driven explicitly via getTimeline(...)->advanceAndApply(...).
   *
   * Inherited from PAGComposition: the file root spawns no timelines of its own, so advance()
   * only recurses into the child compositions. When mixing multiple top-level animations, drive
   * each timeline via getTimeline(id)->advance(dt) / apply(mix) yourself, then call advance(dt)
   * once for the compositions.
   */
  using PAGComposition::advance;

  /**
   * Re-applies every runtime composition (recursing into nested child compositions) at its
   * current time, without advancing. Useful after a new PAGSurface is bound. Inherited from
   * PAGComposition.
   */
  using PAGComposition::apply;

  /**
   * Convenience method equivalent to advance(deltaMicroseconds) followed by apply().
   */
  void advanceAndApply(int64_t deltaMicroseconds);

 private:
  PAGFile() = default;

  // PAGXDocument::notifyChange dispatches here.
  void onNodesChanged(const std::vector<Node*>& dirtyNodes);

  // Constructs a PAGTimeline targeting the given animation, applying its writes to the supplied
  // runtime binding and resolving channel targets against contextDoc. Used by
  // PAGComposition::MakeChild for composition-spawned timelines. Caller owns the returned
  // shared_ptr; the file does not register these timelines into timelinesByAnimation.
  std::shared_ptr<PAGTimeline> createCompositionTimeline(Animation* animation,
                                                         RuntimeBinding* binding,
                                                         PAGXDocument* contextDoc);

  RuntimeBinding* mutableBinding();

  void* getDisplayListForOptions() const;

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
};

}  // namespace pagx
