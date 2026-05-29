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

#include "pagx/nodes/Property.h"
#include <string>
#include "pagx/runtime/KeyframeEvaluator.h"

namespace pagx {

template <typename T>
KeyValue TypedProperty<T>::onEvaluateAtFrame(Frame frame) const {
  if (keyframes.empty()) {
    return T{};
  }
  return EvaluateKeyframeSequence<T>(keyframes, static_cast<double>(frame));
}

template <typename T>
KeyValue TypedProperty<T>::onEvaluateAtMicros(int64_t microseconds, float frameRate) const {
  if (keyframes.empty()) {
    return T{};
  }
  double framePosition = frameRate > 0.0f ? static_cast<double>(microseconds) *
                                                static_cast<double>(frameRate) / 1.0e6
                                          : 0.0;
  return EvaluateKeyframeSequence<T>(keyframes, framePosition);
}

template <>
PropertyValueType TypedProperty<float>::valueType() const {
  return PropertyValueType::Float;
}

template <>
PropertyValueType TypedProperty<bool>::valueType() const {
  return PropertyValueType::Bool;
}

template <>
PropertyValueType TypedProperty<int>::valueType() const {
  return PropertyValueType::Int;
}

template <>
PropertyValueType TypedProperty<std::string>::valueType() const {
  return PropertyValueType::String;
}

template <>
PropertyValueType TypedProperty<ImageRef>::valueType() const {
  return PropertyValueType::ImageRef;
}

template <>
PropertyValueType TypedProperty<Color>::valueType() const {
  return PropertyValueType::Color;
}

// Explicit instantiations: must cover every alternative of KeyValue.
template class TypedProperty<float>;
template class TypedProperty<bool>;
template class TypedProperty<int>;
template class TypedProperty<std::string>;
template class TypedProperty<ImageRef>;
template class TypedProperty<Color>;

}  // namespace pagx
