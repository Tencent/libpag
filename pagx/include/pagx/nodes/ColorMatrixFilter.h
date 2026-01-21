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

#include <array>
#include "pagx/nodes/LayerFilter.h"

namespace pagx {

/**
 * A color matrix filter that transforms the layer colors using a 4x5 matrix.
 * The matrix is applied as: [R' G' B' A'] = [R G B A 1] * matrix
 */
class ColorMatrixFilter : public LayerFilter {
 public:
  /**
   * The 4x5 color transformation matrix stored as a 20-element array in row-major order.
   * Elements 0-4: red channel transformation
   * Elements 5-9: green channel transformation
   * Elements 10-14: blue channel transformation
   * Elements 15-19: alpha channel transformation
   * The last element of each row is an additive offset (bias).
   * The default value is the identity matrix.
   */
  std::array<float, 20> matrix = {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0};

  NodeType nodeType() const override {
    return NodeType::ColorMatrixFilter;
  }
};

}  // namespace pagx
