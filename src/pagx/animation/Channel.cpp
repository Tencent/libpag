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

#include "pagx/nodes/Channel.h"
#include <string>
#include "pagx/runtime/KeyframeEvaluator.h"

namespace pagx {

template <typename T>
KeyValue TypedChannel<T>::onEvaluateAtFrame(Frame frame) const {
  return EvaluateKeyframeSequence<T>(keyframes, static_cast<double>(frame));
}

template <typename T>
KeyValue TypedChannel<T>::onEvaluateAtMicros(int64_t microseconds, float frameRate) const {
  double framePosition =
      frameRate > 0.0f ? static_cast<double>(microseconds) * static_cast<double>(frameRate) / 1.0e6
                       : 0.0;
  return EvaluateKeyframeSequence<T>(keyframes, framePosition);
}

template <>
PropertyValueType TypedChannel<float>::valueType() const {
  return PropertyValueType::Float;
}

template <>
PropertyValueType TypedChannel<bool>::valueType() const {
  return PropertyValueType::Bool;
}

template <>
PropertyValueType TypedChannel<int>::valueType() const {
  return PropertyValueType::Int;
}

template <>
PropertyValueType TypedChannel<std::string>::valueType() const {
  return PropertyValueType::String;
}

template <>
PropertyValueType TypedChannel<ImageRef>::valueType() const {
  return PropertyValueType::ImageRef;
}

template <>
PropertyValueType TypedChannel<Color>::valueType() const {
  return PropertyValueType::Color;
}

// Explicit instantiations: must cover every alternative of KeyValue.
template class TypedChannel<float>;
template class TypedChannel<bool>;
template class TypedChannel<int>;
template class TypedChannel<std::string>;
template class TypedChannel<ImageRef>;
template class TypedChannel<Color>;

}  // namespace pagx
