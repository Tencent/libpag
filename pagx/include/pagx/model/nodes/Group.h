/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <memory>
#include <string>
#include <vector>
#include "pagx/model/types/Types.h"
#include "pagx/model/nodes/VectorElement.h"

namespace pagx {

/**
 * Group container.
 */
struct Group : public VectorElement {
  std::string name = {};
  Point anchorPoint = {};
  Point position = {};
  float rotation = 0;
  Point scale = {1, 1};
  float skew = 0;
  float skewAxis = 0;
  float alpha = 1;
  std::vector<std::unique_ptr<VectorElement>> elements = {};

  NodeType type() const override {
    return NodeType::Group;
  }

  std::unique_ptr<Node> clone() const override {
    auto node = std::make_unique<Group>();
    node->name = name;
    node->anchorPoint = anchorPoint;
    node->position = position;
    node->rotation = rotation;
    node->scale = scale;
    node->skew = skew;
    node->skewAxis = skewAxis;
    node->alpha = alpha;
    for (const auto& element : elements) {
      node->elements.push_back(
          std::unique_ptr<VectorElement>(static_cast<VectorElement*>(element->clone().release())));
    }
    return node;
  }
};

}  // namespace pagx
