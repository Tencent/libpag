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
#include <cstdint>
#include <string>
#include <variant>
#include <vector>
#include "pagx/animation/Keyframe.h"
#include "pagx/nodes/Node.h"

namespace pagx {

using KeyValue = std::variant<float, bool, int, std::string, ImageRef, Color>;

class Property : public Node {
 public:
  std::string channel = {};

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

 private:
  KeyValue onEvaluateAtFrame(Frame frame) const override {
    if (keyframes.empty()) {
      return T{};
    }
    auto it = std::upper_bound(keyframes.begin(), keyframes.end(), frame,
                               [](Frame value, const Keyframe<T>& keyframe) {
                                 return value < keyframe.time;
                               });
    if (it == keyframes.begin()) {
      return it->value;
    }
    --it;
    return it->value;
  }

  KeyValue onEvaluateAtMicros(int64_t microseconds, float frameRate) const override {
    if (keyframes.empty()) {
      return T{};
    }
    // Compute continuous frame position in double precision so callers driving the timeline by
    // microsecond deltas don't lose precision in the floor-to-Frame conversion.
    double framePosition = frameRate > 0.0f ? static_cast<double>(microseconds) *
                                                  static_cast<double>(frameRate) / 1.0e6
                                            : 0.0;
    auto it = std::upper_bound(keyframes.begin(), keyframes.end(), framePosition,
                               [](double value, const Keyframe<T>& keyframe) {
                                 return value < static_cast<double>(keyframe.time);
                               });
    if (it == keyframes.begin()) {
      return it->value;
    }
    --it;
    // PR8 will replace this Hold-only baseline with Linear/Bezier interpolation that uses
    // framePosition for high precision.
    return it->value;
  }

  friend class PAGXDocument;
};

}  // namespace pagx
