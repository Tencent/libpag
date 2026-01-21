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

#include "pagx/nodes/Element.h"
#include "pagx/nodes/Point.h"
#include "pagx/nodes/Size.h"

namespace pagx {

/**
 * Ellipse represents an ellipse shape defined by a center point and size.
 */
class Ellipse : public Element {
 public:
  /**
   * The center point of the ellipse.
   */
  Point center = {};

  /**
   * The size of the ellipse. The default value is {100, 100}.
   */
  Size size = {100, 100};

  /**
   * Whether the path direction is reversed. The default value is false.
   */
  bool reversed = false;

  NodeType nodeType() const override {
    return NodeType::Ellipse;
  }
};

}  // namespace pagx
