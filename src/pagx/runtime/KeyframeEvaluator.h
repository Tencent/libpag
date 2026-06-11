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
#include <cmath>
#include <string>
#include "pagx/nodes/Keyframe.h"
#include "pagx/runtime/BezierEasing.h"
#include "pagx/types/Color.h"
#include "pagx/types/Matrix.h"
#include "pagx/utils/ColorSpaceUtils.h"

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

template <>
inline Color LerpKeyframeValue<Color>(const Color& a, const Color& b, double t) {
  return LerpColor(a, b, t);
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

// Decomposed form of a 2D affine matrix used for interpolation. Interpolating these components
// independently (rather than the raw matrix entries) keeps rotation angular and scale uniform so a
// matrix tween follows the natural translate/rotate/scale/skew path instead of shearing through
// intermediate non-orthogonal states.
struct DecomposedMatrix {
  float translateX = 0;
  float translateY = 0;
  float rotation = 0;  // radians
  float scaleX = 1;
  float scaleY = 1;
  float skew = 0;  // radians, horizontal shear after rotation
};

// Decomposes a 2D affine matrix into translate / rotation / scale / skew. Uses the standard QR-like
// decomposition of the [a c; b d] linear part: the first basis column gives rotation and scaleX,
// the shear of the second column gives skew, and the remaining magnitude gives scaleY.
inline DecomposedMatrix DecomposeMatrix(const Matrix& m) {
  DecomposedMatrix out = {};
  out.translateX = m.tx;
  out.translateY = m.ty;
  float scaleX = std::sqrt(m.a * m.a + m.b * m.b);
  float rotation = std::atan2(m.b, m.a);
  // Remove rotation from the second column to expose shear and scaleY.
  float cosR = std::cos(rotation);
  float sinR = std::sin(rotation);
  float shearedC = cosR * m.c + sinR * m.d;
  float shearedD = -sinR * m.c + cosR * m.d;
  float scaleY = shearedD;
  float skew = scaleY != 0.0f ? (shearedC / scaleY) : 0.0f;
  out.rotation = rotation;
  out.scaleX = scaleX;
  out.scaleY = scaleY;
  out.skew = std::atan(skew);
  return out;
}

// Recomposes a 2D affine matrix from decomposed components, inverting DecomposeMatrix.
inline Matrix RecomposeMatrix(const DecomposedMatrix& d) {
  float cosR = std::cos(d.rotation);
  float sinR = std::sin(d.rotation);
  float tanSkew = std::tan(d.skew);
  // Linear part = Rotation * Skew * Scale, matching the decomposition order.
  float a = cosR * d.scaleX;
  float b = sinR * d.scaleX;
  float c = (cosR * tanSkew - sinR) * d.scaleY;
  float dd = (sinR * tanSkew + cosR) * d.scaleY;
  Matrix m = {};
  m.a = a;
  m.b = b;
  m.c = c;
  m.d = dd;
  m.tx = d.translateX;
  m.ty = d.translateY;
  return m;
}

template <>
inline Matrix LerpKeyframeValue<Matrix>(const Matrix& a, const Matrix& b, double t) {
  auto da = DecomposeMatrix(a);
  auto db = DecomposeMatrix(b);
  DecomposedMatrix mixed = {};
  mixed.translateX = static_cast<float>(da.translateX + (db.translateX - da.translateX) * t);
  mixed.translateY = static_cast<float>(da.translateY + (db.translateY - da.translateY) * t);
  mixed.rotation = static_cast<float>(da.rotation + (db.rotation - da.rotation) * t);
  mixed.scaleX = static_cast<float>(da.scaleX + (db.scaleX - da.scaleX) * t);
  mixed.scaleY = static_cast<float>(da.scaleY + (db.scaleY - da.scaleY) * t);
  mixed.skew = static_cast<float>(da.skew + (db.skew - da.skew) * t);
  return RecomposeMatrix(mixed);
}

// Comparator for std::upper_bound: returns true when framePosition precedes the keyframe's time.
// Defined as a named function template because the project forbids lambdas.
template <typename T>
static bool FramePositionBeforeKeyframe(double value, const Keyframe<T>& kf) {
  return value < static_cast<double>(kf.time);
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
  auto upper = std::upper_bound(keyframes.begin(), keyframes.end(), framePosition,
                                FramePositionBeforeKeyframe<T>);
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
