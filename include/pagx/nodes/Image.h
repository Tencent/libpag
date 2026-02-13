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
#include "pagx/types/Data.h"
#include "pagx/nodes/Node.h"

namespace pagx {

/**
 * Image represents an image resource that can be referenced by other nodes. The image source can
 * be a file path, a URL, or a base64-encoded data URI.
 */
class Image : public Node {
 public:
  /**
   * Image binary data (decoded from base64).
   */
  std::shared_ptr<Data> data = nullptr;

  /**
   * External file path (mutually exclusive with data, data has priority).
   */
  std::string filePath = {};

  static bool IsTypeOf(NodeType type) {
    return type == NodeType::Image;
  }

  NodeType nodeType() const override {
    return NodeType::Image;
  }

 private:
  Image() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
