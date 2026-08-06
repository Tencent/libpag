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
#include <string>
#include <utility>
#include "pagx/nodes/Timeline.h"

namespace pagx {

struct RuntimeBinding;
class PAGScene;
class PAGXDocument;

/**
 * PAGTimeline is the abstract base for runtime playback drivers. A PAGAnimation plays a single
 * Animation clip; a PAGStateMachine drives a state machine that switches between clips. Both share
 * the advance/apply contract used by the owning PAGComposition to drive per-frame playback.
 *
 * Obtain concrete instances through PAGScene: getAnimation() / getStateMachineTimeline() return the
 * typed subclasses; getDefaultTimeline() returns whichever comes first in the document's
 * &lt;Animations&gt; block.
 *
 * Thread safety: PAGTimeline and its subclasses are not thread-safe. Calls on a given timeline (and
 * on the PAGScene that owns it) must be serialized by the caller.
 */
class PAGTimeline {
 public:
  virtual ~PAGTimeline() = default;

  /**
   * Returns the concrete timeline kind, for dispatch without dynamic_cast.
   */
  virtual TimelineType type() const = 0;

  /**
   * Returns the playback identifier (the animation or state machine id). Returns an empty string
   * once the owning PAGScene is destroyed.
   */
  virtual const std::string& getId() const = 0;

  /**
   * Advances the playback state by deltaMicroseconds. Does not change the content; call apply() to
   * reflect the new state.
   * @param deltaMicroseconds the elapsed time in microseconds. May be negative.
   * @return true if the playback state changed.
   */
  virtual bool advance(int64_t deltaMicroseconds) = 0;

  /**
   * Applies the current playback state to the content, blended with existing values by mix.
   * @param mix blend weight, defaults to 1.0 (full overwrite).
   */
  virtual void apply(float mix = 1.0f) = 0;

  /**
   * Convenience method equivalent to advance(deltaMicroseconds) followed by apply(mix). Returns
   * the result of advance(deltaMicroseconds).
   */
  bool advanceAndApply(int64_t deltaMicroseconds, float mix = 1.0f) {
    bool changed = advance(deltaMicroseconds);
    apply(mix);
    return changed;
  }

 protected:
  PAGTimeline(RuntimeBinding* binding, PAGXDocument* contextDoc, std::weak_ptr<PAGScene> owner)
      : owner(std::move(owner)), binding(binding), contextDoc(contextDoc) {
  }

  // Resolves the binding this timeline should write through. A non-null binding is a fixed
  // composition binding co-owned with that composition. A null binding marks a top-level timeline:
  // the owning scene's current root binding is resolved lazily here, so the timeline keeps working
  // after the scene rebuilds its runtime tree (which frees and replaces that binding) instead of
  // dereferencing a dangling pointer. Returns nullptr once the owning scene is gone.
  RuntimeBinding* effectiveBinding() const;

  // Owning scene. binding / contextDoc point into content this scene keeps alive, so subclasses
  // bail out of advance()/apply() once the scene is gone to avoid dereferencing freed memory.
  std::weak_ptr<PAGScene> owner;
  // Runtime binding the channel writers should target. Null marks a top-level timeline resolved
  // lazily via effectiveBinding(); non-null is a fixed composition binding.
  RuntimeBinding* binding = nullptr;
  // Document used to resolve channel target IDs. Top-level timelines use the scene's primary
  // document; timelines spawned by external composition layers use the layer's externalDoc so
  // internal IDs of the external file stay self-contained.
  PAGXDocument* contextDoc = nullptr;
};

}  // namespace pagx
