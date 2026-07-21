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
#include "pagx/runtime/MatrixDecompose.h"
#include "pagx/types/Color.h"
#include "pagx/types/Matrix.h"
#include "pagx/utils/ColorSpaceUtils.h"

namespace pagx {

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

template <>
inline Matrix LerpKeyframeValue<Matrix>(const Matrix& a, const Matrix& b, double t) {
  auto da = DecomposeAffine(a.a, a.b, a.c, a.d, a.tx, a.ty);
  auto db = DecomposeAffine(b.a, b.b, b.c, b.d, b.tx, b.ty);
  auto mixed = MixDecomposed(da, db, static_cast<float>(t));
  Matrix result = {};
  RecomposeAffine(mixed, &result.a, &result.b, &result.c, &result.d, &result.tx, &result.ty);
  return result;
}

template <typename T>
static bool FramePositionBeforeKeyframe(double value, const Keyframe<T>& kf) {
  return value < static_cast<double>(kf.time);
}

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
