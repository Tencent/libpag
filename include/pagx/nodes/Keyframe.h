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
   * No interpolation; the keyframe carries no easing information and holds its value until the
   * next keyframe time, behaving the same as Hold.
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
  /**
   * Spring easing using this keyframe's spring parameters over the segment to the next keyframe.
   */
  Spring = 4,
};

/**
 * SpringEasingModel selects the parameter-to-curve mapping used by Spring interpolation. The model
 * is part of the document format: a later version may map the same parameters to a different
 * curve, so readers must reject models they do not implement instead of falling back.
 */
enum class SpringEasingModel : uint8_t {
  /**
   * Interval-normalized spring. The segment duration is one normalized time unit; bounce maps to
   * damping (0 = critical, positive = underdamped, negative = overdamped) and the decay is chosen
   * so both the displacement and velocity residuals at the segment end stay within the settle
   * tolerance.
   */
  NormalizedV1 = 0,
  /**
   * Physical spring evaluated in real time. The segment duration in seconds (frames divided by
   * frameRate) is the evaluation window; stiffness, damping and mass map to the second-order step
   * response. The segment should be long enough for the spring to settle, otherwise the response
   * is cut off at the segment end.
   */
  PhysicalV1 = 1,
};

/**
 * The supported range of SpringEasing::bounce. Figma exposes [0, 1]; negative values are an Ardot
 * extension for overdamped springs. The lower bound keeps the overdamped response from becoming
 * arbitrarily slow.
 */
constexpr float MinSpringBounce = -0.5f;
constexpr float MaxSpringBounce = 1.0f;

/**
 * SpringEasing holds the parameters of one spring-eased segment. The parameters belong to the
 * outgoing segment of the keyframe that declares them.
 */
struct SpringEasing {
  /**
   * Damping control in [MinSpringBounce, MaxSpringBounce]: 0 is critically damped, positive
   * values overshoot, negative values approach the target without overshooting. Used by
   * NormalizedV1 only.
   */
  float bounce = 0.0f;

  /**
   * Spring stiffness, in force units per unit displacement. Positive; used by PhysicalV1 only.
   */
  float stiffness = 0.0f;

  /**
   * Viscous damping coefficient. Non-negative; zero yields an undamped oscillation. Used by
   * PhysicalV1 only.
   */
  float damping = 0.0f;

  /**
   * Mass of the oscillating body. Positive; used by PhysicalV1 only.
   */
  float mass = 0.0f;

  /**
   * The parameter-to-curve mapping used to evaluate the spring.
   */
  SpringEasingModel model = SpringEasingModel::NormalizedV1;
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
 * Keyframe holds a single animated value at a specific time, together with the interpolation mode,
 * bezier handles and spring parameters used to reach the next keyframe.
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

  /**
   * The spring parameters of this keyframe, used when interpolation is Spring. They describe the
   * segment from this keyframe to the next and are ignored by other interpolation types.
   */
  SpringEasing spring = {};
};

}  // namespace pagx
