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

#include <algorithm>
#include <string>
#include "pagx/nodes/Keyframe.h"
#include "pagx/runtime/BezierEasing.h"
#include "pagx/types/Color.h"

namespace pagx {

// Continuous lerp for value types that support smooth interpolation. Discrete types specialize
// below to ignore the eased fraction and return the start value (Hold semantics). The primary
// template is intentionally unimplemented — every supported KeyValue alternative must specialize.
template <typename T>
T LerpKeyframeValue(const T& a, const T& b, double t) = delete;

template <>
inline float LerpKeyframeValue<float>(const float& a, const float& b, double t) {
  return static_cast<float>(a + (b - a) * t);
}

// Linearly interpolate between two float values by fraction t. Used by LerpKeyframeValue<Color>
// to interpolate each color component.
inline float LerpFloat(float a, float b, double t) {
  return static_cast<float>(a + (static_cast<double>(b) - a) * t);
}

template <>
inline Color LerpKeyframeValue<Color>(const Color& a, const Color& b, double t) {
  Color result = a;
  result.red = LerpFloat(a.red, b.red, t);
  result.green = LerpFloat(a.green, b.green, t);
  result.blue = LerpFloat(a.blue, b.blue, t);
  result.alpha = LerpFloat(a.alpha, b.alpha, t);
  result.colorSpace = b.colorSpace;
  return result;
}

template <>
inline bool LerpKeyframeValue<bool>(const bool& a, const bool& /*b*/, double /*t*/) {
  return a;
}

template <>
inline int LerpKeyframeValue<int>(const int& a, const int& /*b*/, double /*t*/) {
  return a;
}

template <>
inline std::string LerpKeyframeValue<std::string>(const std::string& a, const std::string& /*b*/,
                                                  double /*t*/) {
  return a;
}

template <>
inline ImageRef LerpKeyframeValue<ImageRef>(const ImageRef& a, const ImageRef& /*b*/,
                                            double /*t*/) {
  return a;
}

// Evaluates a Hold/None/Linear/Bezier-segmented keyframe sequence at the given continuous frame
// position. Boundary policy is clamp-to-end: positions outside [keyframes.front().time,
// keyframes.back().time] return the boundary value.
//
// `interpolation` and bezier control points come from `keyframes[i]` (the segment start) per the
// plan's ownership rule: keyframes[i].interpolation describes the i -> i+1 segment, with
// keyframes[i].bezierOut and keyframes[i+1].bezierIn forming the Bezier handles.
template <typename T>
T EvaluateKeyframeSequence(const std::vector<Keyframe<T>>& keyframes, double framePosition) {
  if (keyframes.empty()) {
    return T{};
  }
  if (framePosition <= static_cast<double>(keyframes.front().time)) {
    return keyframes.front().value;
  }
  if (framePosition >= static_cast<double>(keyframes.back().time)) {
    return keyframes.back().value;
  }
  // Locate the segment whose right endpoint exceeds framePosition.
  auto upper = std::upper_bound(
      keyframes.begin(), keyframes.end(), framePosition,
      [](double value, const Keyframe<T>& kf) { return value < static_cast<double>(kf.time); });
  auto right = upper;
  auto left = upper - 1;
  double leftTime = static_cast<double>(left->time);
  double rightTime = static_cast<double>(right->time);
  double span = rightTime - leftTime;
  if (span <= 0.0) {
    return left->value;
  }
  double rawT = (framePosition - leftTime) / span;
  switch (left->interpolation) {
    case KeyframeInterpolationType::Hold:
      return left->value;
    case KeyframeInterpolationType::Bezier: {
      double eased = SolveBezierEasing(
          static_cast<double>(left->bezierOut.x), static_cast<double>(left->bezierOut.y),
          static_cast<double>(right->bezierIn.x), static_cast<double>(right->bezierIn.y), rawT);
      return LerpKeyframeValue<T>(left->value, right->value, eased);
    }
    case KeyframeInterpolationType::None:
    case KeyframeInterpolationType::Linear:
    default:
      return LerpKeyframeValue<T>(left->value, right->value, rawT);
  }
}

}  // namespace pagx
