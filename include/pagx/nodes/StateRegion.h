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
#include "pagx/nodes/Node.h"

namespace pagx {

class State;
class StateTransition;

/**
 * StateRegion is one orthogonal region of a StateMachine. It owns an independent set of States
 * and StateTransitions and its own initial state, and advances in parallel with sibling regions.
 * Multiple regions let orthogonal dimensions coexist without a combinatorial state explosion.
 */
class StateRegion : public Node {
 public:
  /**
   * The region name, unique within the owning StateMachine. Used by runtime to query this
   * region's current state (e.g. getCurrentState(regionName)).
   */
  std::string name = {};

  /**
   * The name of the state this region enters when the state machine starts. If no matching State
   * is found, the first State in the region is used as the default.
   */
  std::string initialState = {};

  /**
   * The states belonging to this region. State names are unique within the region (different
   * regions may reuse the same state name).
   */
  std::vector<State*> states = {};

  /**
   * The transitions belonging to this region. Each transition's from/to reference state names
   * within this region; the special from value "any" matches any state in this region.
   */
  std::vector<StateTransition*> transitions = {};

  NodeType nodeType() const override {
    return NodeType::StateRegion;
  }

 private:
  StateRegion() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
