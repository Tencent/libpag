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
#include <vector>
#include "pagx/PAGAnimation.h"
#include "pagx/PAGStateMachine.h"
#include "pagx/nodes/Keyframe.h"
#include "pagx/nodes/State.h"
#include "pagx/nodes/StateRegion.h"
#include "pagx/nodes/StateTransition.h"

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

}  // namespace pagx
