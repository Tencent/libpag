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
#include <variant>
#include <vector>
#include "pagx/animation/Keyframe.h"
#include "pagx/nodes/Node.h"

namespace pagx {

using KeyValue = std::variant<float, bool, int, std::string, ImageRef, Color>;

class Property : public Node {
 public:
  std::string channel = {};

  KeyValue evaluateAt(Frame frame) const {
    return onEvaluateAt(frame);
  }

  NodeType nodeType() const override {
    return NodeType::Property;
  }

 protected:
  Property() = default;

 private:
  virtual KeyValue onEvaluateAt(Frame frame) const = 0;

  friend class PAGXDocument;
};

template <typename T>
class TypedProperty : public Property {
 public:
  std::vector<Keyframe<T>> keyframes = {};

 private:
  KeyValue onEvaluateAt(Frame frame) const override {
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

  friend class PAGXDocument;
};

}  // namespace pagx
