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
#include <string>
#include "pagx/nodes/Node.h"

namespace pagx {

/**
 * StateType discriminates State subclasses for runtime dispatch without dynamic_cast. The v1
 * implementation only ships AnimationState; future kinds (e.g. BlendState) extend this enum.
 */
enum class StateType : uint8_t {
  /**
   * A state bound to a single Animation (or to nothing, an empty control-flow state).
   */
  Animation = 0,
};

/**
 * State is the base for a single state within a StateRegion. Concrete kinds are distinguished by
 * stateType(). A State with no bound animation acts as an empty control-flow state (its advance /
 * apply are no-ops), useful as a logic-only stepping stone.
 */
class State : public Node {
 public:
  /**
   * The state name, unique within the owning StateRegion. Referenced by StateTransition.from/to.
   */
  std::string name = {};

  /**
   * Returns the concrete state kind, used by runtime dispatch.
   */
  virtual StateType stateType() const = 0;

  NodeType nodeType() const override {
    return NodeType::State;
  }

 protected:
  State() = default;
};

/**
 * AnimationState plays a referenced Animation while it is the current state of its region. The
 * animation is optional: when animationId is empty the state produces no output (empty state).
 */
class AnimationState : public State {
 public:
  /**
   * The id of the Animation this state plays, resolved against the owning document's id table.
   * Empty means an empty state that drives no channels.
   */
  std::string animationId = {};

  StateType stateType() const override {
    return StateType::Animation;
  }

 private:
  AnimationState() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
