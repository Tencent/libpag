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
#include "pagx/timeline/Timeline.h"

namespace pagx {

/**
 * AnimationTimeline attaches a referenced Animation to the owning Layer's runtime composition.
 * The Animation is resolved via PAGXDocument::findNode<Animation>(animationId) at build time;
 * the id must match an Animation defined anywhere in the same document (animation ids are unique
 * across the entire document, not scoped to a single Composition).
 */
class AnimationTimeline : public Timeline {
 public:
  AnimationTimeline() = default;

  /**
   * The id of the referenced Animation. Resolved against the owning document's id table.
   */
  std::string animationId = {};

  /**
   * Initial playing state when the embedding PAGFile first builds the runtime tree. Setting this
   * to false constructs the timeline in the paused state; callers can call play() later. Default
   * is true.
   */
  bool playing = true;

  TimelineType timelineType() const override {
    return TimelineType::Animation;
  }
};

}  // namespace pagx
