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
#include "pagx/nodes/Keyframe.h"
#include "pagx/nodes/Node.h"

namespace pagx {

class AnimationObject;

/**
 * LoopMode selects how an Animation repeats once it reaches the end of its duration.
 */
enum class LoopMode {
  /**
   * Plays once and stops at the final frame.
   */
  Once,
  /**
   * Restarts from the beginning each time it reaches the end.
   */
  Loop,
  /**
   * Plays forward to the end, then backward to the start, repeating indefinitely.
   */
  PingPong,
};

/**
 * Animation defines a named timeline composed of Object/Channel/Keyframe entries. The animation
 * is identified by Node::id, which must be unique within the owning PAGXDocument. References from
 * Layer.timelines drivers look up animations via this id.
 */
class Animation : public Node {
 public:
  /**
   * The total length of the animation, in frames. Together with startOffset it defines the
   * visibility window [startOffset, startOffset + duration) of the content this animation drives.
   */
  Frame duration = 0;

  /**
   * The frame position (in this animation's own frameRate) at which this animation begins on the
   * owning composition's timeline. Before startOffset and after startOffset + duration, the target
   * nodes this animation drives are hidden. Carries the semantics of PAG's Layer.startTime combined
   * with the layer visibility window. Default is 0 (visible from the first frame).
   */
  Frame startOffset = 0;

  /**
   * The frame rate used to convert between time and frames when sampling keyframes.
   */
  float frameRate = 60.0f;

  /**
   * The loop behavior applied once the animation reaches its duration.
   */
  LoopMode loop = LoopMode::Once;

  /**
   * The animated objects that make up this animation, each driving a single target node.
   */
  std::vector<AnimationObject*> objects = {};

  NodeType nodeType() const override {
    return NodeType::Animation;
  }

 private:
  Animation() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
