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
//  license is distributed on an "as IS" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include "pagx/nodes/Timeline.h"

namespace pagx {

/**
 * PAGTimeline is the abstract base for runtime playback drivers. A PAGAnimation plays a single
 * Animation clip; a PAGStateMachine drives a state machine that switches between clips. Both share
 * the advance/apply contract used by the owning PAGComposition to drive per-frame playback.
 *
 * Obtain concrete instances through PAGScene: getTimeline() / getStateMachineTimeline() return the
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
  virtual bool advanceAndApply(int64_t deltaMicroseconds, float mix = 1.0f) = 0;
};

}  // namespace pagx
