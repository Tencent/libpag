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

#include <vector>
#include "pagx/nodes/ColorSource.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/Matrix.h"
#include "pagx/nodes/Point.h"

namespace pagx {

/**
 * A diamond gradient color source that produces a gradient in a diamond shape from the center.
 */
class DiamondGradient : public ColorSource {
 public:
  /**
   * The center point of the gradient.
   */
  Point center = {};

  /**
   * Half the diagonal length of the diamond shape.
   */
  float radius = 0;

  /**
   * The transformation matrix applied to the gradient.
   */
  Matrix matrix = {};

  /**
   * The color stops defining the gradient colors and positions.
   */
  std::vector<ColorStop> colorStops = {};

  NodeType nodeType() const override {
    return NodeType::DiamondGradient;
  }

 private:
  DiamondGradient() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
