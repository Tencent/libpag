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
#include <variant>
#include <vector>
#include "pagx/animation/Keyframe.h"
#include "pagx/nodes/Node.h"

namespace pagx {

using KeyValue = std::variant<float, bool, int, std::string, ImageRef, Color>;

/**
 * Discriminator for the value type carried by a Property's keyframes. Aligned with the order of
 * KeyValue's std::variant alternatives.
 */
enum class PropertyValueType : uint8_t {
  Float = 0,
  Bool = 1,
  Int = 2,
  String = 3,
  ImageRef = 4,
  Color = 5,
};

class Property : public Node {
 public:
  std::string channel = {};

  /**
   * Returns the value type of this Property's keyframes. Each TypedProperty<T> specialization
   * reports a distinct PropertyValueType so callers can dispatch on the concrete type without
   * dynamic_cast or RTTI.
   */
  virtual PropertyValueType valueType() const = 0;

  // Evaluates the property value at the given Frame index. Useful for unit tests and any caller
  // operating in frame-discrete time.
  KeyValue evaluateAt(Frame frame) const {
    return onEvaluateAtFrame(frame);
  }

  // Evaluates the property value at the given time in microseconds, using the supplied frameRate
  // to convert microseconds to a continuous frame position. Implementations should perform
  // interpolation in double precision to avoid losing precision when the timeline is being driven
  // by microsecond deltas.
  KeyValue evaluateAt(int64_t microseconds, float frameRate) const {
    return onEvaluateAtMicros(microseconds, frameRate);
  }

  NodeType nodeType() const override {
    return NodeType::Property;
  }

 protected:
  Property() = default;

 private:
  virtual KeyValue onEvaluateAtFrame(Frame frame) const = 0;
  virtual KeyValue onEvaluateAtMicros(int64_t microseconds, float frameRate) const = 0;

  friend class PAGXDocument;
};

template <typename T>
class TypedProperty : public Property {
 public:
  std::vector<Keyframe<T>> keyframes = {};

  PropertyValueType valueType() const override;

 private:
  // Defined in src/pagx/animation/Property.cpp with explicit instantiations for each KeyValue
  // alternative (float, bool, int, std::string, ImageRef, Color). The implementation runs through
  // the runtime KeyframeEvaluator helper so that bezier easing and Color/float lerp logic stay
  // out of this public header.
  KeyValue onEvaluateAtFrame(Frame frame) const override;
  KeyValue onEvaluateAtMicros(int64_t microseconds, float frameRate) const override;

  friend class PAGXDocument;
};

}  // namespace pagx
