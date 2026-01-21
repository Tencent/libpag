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
#include "pagx/model/Node.h"

namespace pagx {

struct Layer;

/**
 * Base class for resource nodes.
 * Resources are nodes that can be defined in the Resources section and referenced by id.
 */
class Resource : public Node {
 public:
  std::string id = {};
};

/**
 * Image resource.
 */
struct Image : public Resource {
  std::string source = {};

  NodeType type() const override {
    return NodeType::Image;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<Image>(*this);
  }
};

/**
 * PathData resource - stores reusable path data.
 */
struct PathDataResource : public Resource {
  std::string data = {};  // SVG path data string

  NodeType type() const override {
    return NodeType::PathData;
  }

  std::unique_ptr<Node> clone() const override {
    return std::make_unique<PathDataResource>(*this);
  }
};

/**
 * Composition resource.
 */
struct Composition : public Resource {
  float width = 0;
  float height = 0;
  std::vector<std::unique_ptr<Layer>> layers = {};

  NodeType type() const override {
    return NodeType::Composition;
  }

  std::unique_ptr<Node> clone() const override;
};

}  // namespace pagx
