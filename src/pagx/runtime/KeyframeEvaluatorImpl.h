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
#include <vector>
#include "pagx/nodes/Keyframe.h"
#include "pagx/runtime/BezierEasing.h"
#include "pagx/runtime/Matrix3DDecompose.h"
#include "pagx/runtime/MatrixDecompose.h"
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

// Fixed point of an affine matrix: the point p with M(p) == p. A rotation or scale authored as
// "transform about (x, y)" bakes that pivot into the translation, so interpolating the translation
// linearly drags the pivot away from its authored position mid-tween. Returns false when the linear
// part has no unique fixed point (pure translation, singular scale).
inline bool SolveMatrixFixedPoint(const Matrix& m, float* x, float* y) {
  const float ia = 1.0f - m.a;
  const float ib = -m.b;
  const float ic = -m.c;
  const float id = 1.0f - m.d;
  const float det = ia * id - ic * ib;
  if (std::abs(det) < 1e-4f) {
    return false;
  }
  *x = (id * m.tx - ic * m.ty) / det;
  *y = (ia * m.ty - ib * m.tx) / det;
  return true;
}

// True when the matrix carries neither rotation/scale nor translation.
inline bool IsIdentityTransform(const Matrix& m) {
  return std::abs(m.a - 1.0f) < 1e-5f && std::abs(m.b) < 1e-5f && std::abs(m.c) < 1e-5f &&
         std::abs(m.d - 1.0f) < 1e-5f && std::abs(m.tx) < 1e-3f && std::abs(m.ty) < 1e-3f;
}

template <>
inline Matrix LerpKeyframeValue<Matrix>(const Matrix& a, const Matrix& b, double t) {
  // See MatrixDecompose.h for the winding limitation: matrix rotation interpolation cannot recover
  // full turns, so keyframes needing precise multi-turn or +/-pi-boundary rotation should target a
  // scalar rotation channel (e.g. Group::rotation) instead of a Matrix.
  auto da = DecomposeAffine(a.a, a.b, a.c, a.d, a.tx, a.ty);
  auto db = DecomposeAffine(b.a, b.b, b.c, b.d, b.tx, b.ty);
  auto mixed = MixDecomposed(da, db, static_cast<float>(t));
  Matrix result = {};
  RecomposeAffine(mixed, &result.a, &result.b, &result.c, &result.d, &result.tx, &result.ty);

  // Spell out the translation from the shared pivot instead of lerping it, so rotation and scale
  // happen in place. Sampling interpolates between two poses only, hence a keyframe that starts at
  // the identity pose contributes the other pose's pivot.
  float pivotX = 0.0f;
  float pivotY = 0.0f;
  bool hasPivot = false;
  float ax = 0.0f, ay = 0.0f, bx = 0.0f, by = 0.0f;
  const bool aHasPivot = SolveMatrixFixedPoint(a, &ax, &ay);
  const bool bHasPivot = SolveMatrixFixedPoint(b, &bx, &by);
  if (aHasPivot && bHasPivot && std::abs(ax - bx) < 0.5f && std::abs(ay - by) < 0.5f) {
    hasPivot = true;
    pivotX = ax;
    pivotY = ay;
  } else if (bHasPivot && IsIdentityTransform(a)) {
    hasPivot = true;
    pivotX = bx;
    pivotY = by;
  } else if (aHasPivot && IsIdentityTransform(b)) {
    hasPivot = true;
    pivotX = ax;
    pivotY = ay;
  }
  if (hasPivot) {
    result.tx = pivotX - (result.a * pivotX + result.c * pivotY);
    result.ty = pivotY - (result.b * pivotX + result.d * pivotY);
  }
  return result;
}

template <>
inline Matrix3D LerpKeyframeValue<Matrix3D>(const Matrix3D& a, const Matrix3D& b, double t) {
  return LerpMatrix3D(a, b, t);
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
    case KeyframeInterpolationType::None:
    case KeyframeInterpolationType::Hold:
      return left->value;
    case KeyframeInterpolationType::Bezier:
      return LerpKeyframeValue<T>(left->value, right->value,
                                  SolveBezierEasing(static_cast<double>(left->bezierOut.x),
                                                    static_cast<double>(left->bezierOut.y),
                                                    static_cast<double>(right->bezierIn.x),
                                                    static_cast<double>(right->bezierIn.y), rawT));
    case KeyframeInterpolationType::Linear:
    default:
      return LerpKeyframeValue<T>(left->value, right->value, rawT);
  }
}

}  // namespace pagx
