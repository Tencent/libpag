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
#include "pagx/runtime/BezierEasing.h"
#include "pagx/runtime/KeyframeEvaluatorImpl.h"

namespace pagx {

// Type-erased lerp dispatcher. Delegates to LerpKeyframeValue<T> template specializations
// defined in KeyframeEvaluatorImpl.h to avoid duplicate interpolation logic.
static KeyValue EvaluateLerp(const KeyValue& a, const KeyValue& b, double t) {
  if (a.index() != b.index()) {
    return a;
  }
  switch (static_cast<ChannelValueType>(a.index())) {
    case ChannelValueType::Float:
      return LerpKeyframeValue<float>(std::get<float>(a), std::get<float>(b), t);
    case ChannelValueType::Bool:
      return LerpKeyframeValue<bool>(std::get<bool>(a), std::get<bool>(b), t);
    case ChannelValueType::Int:
      return LerpKeyframeValue<int>(std::get<int>(a), std::get<int>(b), t);
    case ChannelValueType::String:
      return LerpKeyframeValue<std::string>(std::get<std::string>(a), std::get<std::string>(b), t);
    case ChannelValueType::ImageRef:
      return LerpKeyframeValue<ImageRef>(std::get<ImageRef>(a), std::get<ImageRef>(b), t);
    case ChannelValueType::Color:
      return LerpKeyframeValue<Color>(std::get<Color>(a), std::get<Color>(b), t);
    case ChannelValueType::Matrix:
      return LerpKeyframeValue<Matrix>(std::get<Matrix>(a), std::get<Matrix>(b), t);
    case ChannelValueType::PAGImage:
      return std::get<std::shared_ptr<PAGImage>>(a);
  }
  return a;
}

KeyValue EvaluateKeyframeSegment(const KeyValue& leftValue, const KeyValue& rightValue,
                                 double progress, KeyframeInterpolationType interp,
                                 const Point* bezierOut, const Point* bezierIn) {
  switch (interp) {
    case KeyframeInterpolationType::None:
    case KeyframeInterpolationType::Hold:
      return leftValue;
    case KeyframeInterpolationType::Linear:
      return EvaluateLerp(leftValue, rightValue, progress);
    case KeyframeInterpolationType::Bezier:
      if (bezierOut == nullptr || bezierIn == nullptr) {
        return EvaluateLerp(leftValue, rightValue, progress);
      }
      return EvaluateLerp(
          leftValue, rightValue,
          SolveBezierEasing(static_cast<double>(bezierOut->x), static_cast<double>(bezierOut->y),
                            static_cast<double>(bezierIn->x), static_cast<double>(bezierIn->y),
                            progress));
  }
  return leftValue;
}

}  // namespace pagx
