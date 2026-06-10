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
#include "pagx/types/Color.h"
#include "pagx/types/Point.h"

namespace pagx {

/**
 * Frame is a frame-discrete time index used by keyframes and animation durations.
 */
using Frame = int64_t;

/**
 * ZeroFrame is the first frame index (time origin) of an animation.
 */
constexpr Frame ZeroFrame = 0;

/**
 * KeyframeInterpolationType selects how a keyframe's value is interpolated toward the next
 * keyframe.
 */
enum class KeyframeInterpolationType : uint8_t {
  /**
   * No interpolation; reserved for keyframes that carry no easing information.
   */
  None = 0,
  /**
   * Linear interpolation between this keyframe and the next.
   */
  Linear = 1,
  /**
   * Cubic bezier easing using this keyframe's bezierOut and the next keyframe's bezierIn handles.
   */
  Bezier = 2,
  /**
   * Holds this keyframe's value until the next keyframe time, then jumps.
   */
  Hold = 3,
};

/**
 * ImageRef references an image resource by id. Used as a KeyValue alternative for image-valued
 * properties so keyframes can switch between image resources over time.
 */
struct ImageRef {
  /**
   * The id of the referenced image resource, resolved against the owning document's id table.
   */
  std::string id = {};

  bool operator==(const ImageRef& other) const {
    return id == other.id;
  }

  bool operator!=(const ImageRef& other) const {
    return !(*this == other);
  }
};

/**
 * Keyframe holds a single animated value at a specific time, together with the interpolation mode
 * and bezier handles used to reach the next keyframe.
 */
template <typename T>
struct Keyframe {
  /**
   * The time of this keyframe, in frames.
   */
  Frame time = ZeroFrame;

  /**
   * The value at this keyframe.
   */
  T value = {};

  /**
   * The interpolation mode used between this keyframe and the next.
   */
  KeyframeInterpolationType interpolation = KeyframeInterpolationType::Linear;

  /**
   * The outgoing bezier control handle of this keyframe, used when interpolation is Bezier.
   */
  Point bezierOut = {};

  /**
   * The incoming bezier control handle of this keyframe, used when interpolation is Bezier.
   */
  Point bezierIn = {};
};

}  // namespace pagx
