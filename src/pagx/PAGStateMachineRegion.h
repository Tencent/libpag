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
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "pagx/PAGAnimation.h"
#include "pagx/PAGStateMachine.h"
#include "pagx/nodes/Keyframe.h"
#include "pagx/nodes/State.h"
#include "pagx/nodes/StateMachine.h"
#include "pagx/nodes/StateMachineInput.h"
#include "pagx/nodes/StateRegion.h"
#include "pagx/nodes/StateTransition.h"
#include "renderer/LayerBuilder.h"

namespace pagx {

// Converts a frame count to microseconds at the given frame rate. Returns 0 when frameRate is
// non-positive. Shared by PAGAnimation (duration conversion) and PAGStateMachine (crossfade mix).
inline int64_t FramesToUs(Frame frames, float frameRate) {
  if (frameRate <= 0.0f) {
    return 0;
  }
  return static_cast<int64_t>(static_cast<double>(frames) / frameRate * 1000000.0);
}

// Per-region runtime execution state for a PAGStateMachine. Defined in this internal header (not the
// public API) so the implementation and tests can access it, while it stays out of the public
// PAGStateMachine.h interface.
struct PAGStateMachine::RegionInstance {
  struct FadingState {
    std::shared_ptr<PAGAnimation> timeline;
    float mixFrom = 1.0f;
    bool frozen = false;
  };

  const StateRegion* region = nullptr;
  const State* currentState = nullptr;
  std::shared_ptr<PAGAnimation> currentTimeline;
  float mix = 1.0f;
  const StateTransition* transition = nullptr;
  std::vector<FadingState> fadingOut;
  std::set<std::string> consumedTriggers;
};

// A RuntimeTarget subclass that routes channel writes to PAGStateMachine inputs. Each channel
// name corresponds to an SM input name; the target stores the input type map so it can dispatch
// to setBool/setNumber/fireTrigger correctly. This lets DataBindRuntime treat SM inputs the same
// as render node channels — no SM-specific routing needed.
class StateMachineInputTarget : public RuntimeTarget {
 public:
  StateMachineInputTarget(std::shared_ptr<PAGStateMachine> sm, const StateMachine* schema) {
    setObject(sm);
    for (auto* input : schema->inputs) {
      if (input != nullptr) {
        inputTypes[input->name] = input->type;
      }
    }
  }

  bool hasWriter(const std::string& channel) const override {
    return inputTypes.find(channel) != inputTypes.end();
  }

  bool apply(const std::string& channel, const KeyValue& value, float /*mix*/) override {
    auto it = inputTypes.find(channel);
    if (it == inputTypes.end()) {
      return false;
    }
    auto sm = getObject<PAGStateMachine>();
    if (sm == nullptr) {
      return false;
    }
    switch (it->second) {
      case StateMachineInputType::Bool: {
        auto* v = std::get_if<bool>(&value);
        return sm->setBool(channel, v != nullptr ? *v : false);
      }
      case StateMachineInputType::Number: {
        auto* v = std::get_if<float>(&value);
        return sm->setNumber(channel, v != nullptr ? *v : 0.0f);
      }
      case StateMachineInputType::Trigger:
        sm->fireTrigger(channel);
        return true;
    }
    return false;
  }

 private:
  std::unordered_map<std::string, StateMachineInputType> inputTypes = {};
};

}  // namespace pagx
