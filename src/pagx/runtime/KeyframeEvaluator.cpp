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

#include "pagx/runtime/KeyframeEvaluator.h"
#include <variant>
#include "pagx/runtime/BezierEasing.h"
#include "pagx/runtime/MatrixDecompose.h"
#include "pagx/types/ColorSpace.h"
#include "pagx/utils/ColorSpaceUtils.h"

namespace pagx {

// ============================================================================
// Internal typed lerp helpers — NOT part of the public API.
// Discrete types ignore t and return the left value (Hold semantics).
// Continuous types interpolate normally.
// ============================================================================
inline float LerpKeyValueFloat(float a, float b, double t) {
  return a + static_cast<float>((b - a) * t);
}

inline bool LerpKeyValueBool(bool a, bool /*b*/, double /*t*/) {
  return a;
}

inline int LerpKeyValueInt(int a, int /*b*/, double /*t*/) {
  return a;
}

inline std::string LerpKeyValueString(const std::string& a, const std::string& /*b*/,
                                      double /*t*/) {
  return a;
}

inline ImageRef LerpKeyValueImageRef(const ImageRef& a, const ImageRef& /*b*/, double /*t*/) {
  return a;
}

inline Color LerpKeyValueColor(const Color& a, const Color& b, double t) {
  return LerpColor(a, b, t);
}

inline Matrix LerpKeyValueMatrix(const Matrix& a, const Matrix& b, double t) {
  auto da = DecomposeAffine(a.a, a.b, a.c, a.d, a.tx, a.ty);
  auto db = DecomposeAffine(b.a, b.b, b.c, b.d, b.tx, b.ty);
  auto mixed = MixDecomposed(da, db, static_cast<float>(t));
  Matrix result = {};
  RecomposeAffine(mixed, &result.a, &result.b, &result.c, &result.d, &result.tx, &result.ty);
  return result;
}

inline std::shared_ptr<PAGImage> LerpKeyValuePAGImage(const std::shared_ptr<PAGImage>& a,
                                                      const std::shared_ptr<PAGImage>& /*b*/,
                                                      double /*t*/) {
  return a;
}

// Type-erased lerp dispatcher. Calls the appropriate typed helper above based on ChannelValueType.
inline KeyValue EvaluateLerp(const KeyValue& a, const KeyValue& b, double t) {
  if (a.index() != b.index()) {
    return a;
  }
  switch (static_cast<ChannelValueType>(a.index())) {
    case ChannelValueType::Float:
      return LerpKeyValueFloat(std::get<float>(a), std::get<float>(b), t);
    case ChannelValueType::Bool:
      return LerpKeyValueBool(std::get<bool>(a), std::get<bool>(b), t);
    case ChannelValueType::Int:
      return LerpKeyValueInt(std::get<int>(a), std::get<int>(b), t);
    case ChannelValueType::String:
      return LerpKeyValueString(std::get<std::string>(a), std::get<std::string>(b), t);
    case ChannelValueType::ImageRef:
      return LerpKeyValueImageRef(std::get<ImageRef>(a), std::get<ImageRef>(b), t);
    case ChannelValueType::Color:
      return LerpKeyValueColor(std::get<Color>(a), std::get<Color>(b), t);
    case ChannelValueType::Matrix:
      return LerpKeyValueMatrix(std::get<Matrix>(a), std::get<Matrix>(b), t);
    case ChannelValueType::PAGImage:
      return LerpKeyValuePAGImage(std::get<std::shared_ptr<PAGImage>>(a),
                                  std::get<std::shared_ptr<PAGImage>>(b), t);
  }
  return a;
}

KeyValue EvaluateKeyframeSegment(const KeyValue& leftValue, const KeyValue& rightValue,
                                 KeyframeInterpolationType interp, const Point* bezierOut,
                                 const Point* bezierIn, double rawT) {
  switch (interp) {
    case KeyframeInterpolationType::None:
    case KeyframeInterpolationType::Hold:
      return leftValue;
    case KeyframeInterpolationType::Linear:
      return EvaluateLerp(leftValue, rightValue, rawT);
    case KeyframeInterpolationType::Bezier:
      return EvaluateLerp(
          leftValue, rightValue,
          SolveBezierEasing(static_cast<double>(bezierOut->x), static_cast<double>(bezierOut->y),
                            static_cast<double>(bezierIn->x), static_cast<double>(bezierIn->y),
                            rawT));
  }
  return leftValue;
}

}  // namespace pagx
