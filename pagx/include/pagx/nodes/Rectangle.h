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

#include "pagx/nodes/VectorElement.h"
#include "pagx/types/Types.h"

namespace pagx {

/**
 * Rectangle represents a rectangle shape with optional rounded corners.
 */
class Rectangle : public VectorElement {
 public:
  /**
   * The center point of the rectangle.
   */
  Point center = {};

  /**
   * The size of the rectangle. The default value is {100, 100}.
   */
  Size size = {100, 100};

  /**
   * The corner roundness of the rectangle, ranging from 0 to 100. The default value is 0.
   */
  float roundness = 0;

  /**
   * Whether the path direction is reversed. The default value is false.
   */
  bool reversed = false;

  NodeType type() const override {
    return NodeType::Rectangle;
  }

  VectorElementType vectorElementType() const override {
    return VectorElementType::Rectangle;
  }
};

}  // namespace pagx
