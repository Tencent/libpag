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

#include <string>
#include <vector>
#include "pagx/nodes/Keyframe.h"
#include "pagx/nodes/Node.h"
#include "pagx/types/Point.h"

namespace pagx {

class TransitionCondition;

/**
 * The special from-state name that matches any state within the region (UML AnyState). A
 * transition with from == AnyStateName is evaluated regardless of the region's current state.
 */
constexpr const char* AnyStateName = "any";

/**
 * StateTransition describes a directed edge between two states within a StateRegion, together
 * with the conditions that gate it and the crossfade parameters used while switching. All
 * conditions must hold (AND) for the transition to be allowed.
 */
class StateTransition : public Node {
 public:
  /**
   * The source state name, or "any" to match any state in the region.
   */
  std::string from = {};

  /**
   * The destination state name within the region.
   */
  std::string to = {};

  /**
   * The crossfade duration, in frames, over which the outgoing state's weight fades to 0 and the
   * incoming state's weight rises to 1. Converted to time using this transition's own frameRate,
   * so the crossfade speed is independent of which state animations it connects.
   */
  Frame duration = 0;

  /**
   * The frame rate used to convert duration (in frames) to elapsed time during the crossfade.
   */
  float frameRate = 60.0f;

  /**
   * The interpolation curve applied to the crossfade weight over duration. Reuses the keyframe
   * interpolation model (None / Linear / Bezier / Hold).
   */
  KeyframeInterpolationType interpolation = KeyframeInterpolationType::Linear;

  /**
   * The outgoing bezier handle for the crossfade weight curve, used when interpolation is Bezier.
   */
  Point bezierOut = {};

  /**
   * The incoming bezier handle for the crossfade weight curve, used when interpolation is Bezier.
   */
  Point bezierIn = {};

  /**
   * Whether this transition may interrupt an in-progress crossfade. When false (default), the
   * transition is only evaluated once the region has no active crossfade; when true, it can be
   * taken mid-crossfade, and the current outgoing state keeps fading out while the new target
   * fades in (multiple outgoing states may overlap).
   */
  bool enableEarlyExit = false;

  /**
   * Whether the source state's animation is held (frozen) at its current frame when the transition
   * begins, instead of continuing to play during the crossfade. Default false: the source keeps
   * advancing while fading out.
   */
  bool pauseOnExit = false;

  /**
   * The conditions gating this transition. All must evaluate true (AND). An empty list means the
   * transition is always allowed.
   */
  std::vector<TransitionCondition*> conditions = {};

  NodeType nodeType() const override {
    return NodeType::StateTransition;
  }

 private:
  StateTransition() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
