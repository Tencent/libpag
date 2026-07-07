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

#include <vector>
#include "pagx/nodes/Node.h"

namespace pagx {

class StateMachineInput;
class StateRegion;

/**
 * StateMachine defines a named state graph composed of one or more parallel StateRegions plus a
 * set of shared inputs. It is identified by Node::id (unique within the owning PAGXDocument) and
 * referenced from Layer.timelines via StateMachineTimeline.stateMachineId.
 *
 * Each StateRegion runs independently and in parallel (borrowing the orthogonal-region concept
 * from UML state machines), letting orthogonal dimensions (e.g. a button's visual state and its
 * enabled state) be modeled without a combinatorial state explosion. All regions share the same
 * inputs declared here.
 */
class StateMachine : public Node {
 public:
  /**
   * The input ports shared by all regions. Set by host code or synced from a ViewModel. Conditions
   * in any region reference these inputs by name.
   */
  std::vector<StateMachineInput*> inputs = {};

  /**
   * The parallel state regions. At least one is required. Each region owns its states,
   * transitions and initial state, and advances independently.
   */
  std::vector<StateRegion*> regions = {};

  NodeType nodeType() const override {
    return NodeType::StateMachine;
  }

 private:
  StateMachine() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
