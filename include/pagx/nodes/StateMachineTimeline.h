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
#include "pagx/nodes/Timeline.h"

namespace pagx {

/**
 * StateMachineTimeline attaches a referenced StateMachine to the owning Layer's runtime
 * composition. The referenced StateMachine is looked up by stateMachineId against the owning
 * document's id table. Unlike AnimationTimeline, the initial state is not specified here; each
 * StateRegion carries its own initialState.
 */
class StateMachineTimeline : public Timeline {
 public:
  StateMachineTimeline() = default;

  /**
   * The id of the referenced StateMachine. Resolved against the owning document's id table.
   */
  std::string stateMachineId = {};

  TimelineType timelineType() const override {
    return TimelineType::StateMachine;
  }
};

}  // namespace pagx
