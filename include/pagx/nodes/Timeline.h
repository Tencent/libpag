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

namespace pagx {

/**
 * Discriminator for Timeline subclasses. Used by importers, exporters, and runtime layout to
 * dispatch on the concrete timeline kind without dynamic_cast. Future timeline kinds (e.g. time
 * machines, state machines) extend this enum.
 */
enum class TimelineType : uint8_t {
  Animation = 0,
  StateMachine = 1,
};

/**
 * Timeline is the abstract base for entries inside a Layer's <Timelines> XML element. Each entry
 * describes a time-driven behavior to attach when the owning Layer references a Composition.
 * v1 only ships AnimationTimeline; future versions may add other timeline kinds.
 */
class Timeline {
 public:
  virtual ~Timeline() = default;

  /**
   * Returns the concrete timeline kind, used by importers/exporters and runtime dispatch.
   */
  virtual TimelineType timelineType() const = 0;

 protected:
  Timeline() = default;
};

}  // namespace pagx
