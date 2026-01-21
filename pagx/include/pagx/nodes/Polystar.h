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

namespace pagx {

/**
 * Polystar types.
 */
enum class PolystarType {
  Polygon,
  Star
};

/**
 * Polystar represents a polygon or star shape with configurable points, radii, and roundness.
 */
class Polystar : public Element {
 public:
  /**
   * The center point of the polystar.
   */
  Point center = {};

  /**
   * The type of polystar shape, either Star or Polygon. The default value is Star.
   */
  PolystarType type = PolystarType::Star;

  /**
   * The number of points in the polystar. The default value is 5.
   */
  float pointCount = 5;

  /**
   * The outer radius of the polystar. The default value is 100.
   */
  float outerRadius = 100;

  /**
   * The inner radius of the polystar. Only applies when type is Star. The default value
   * is 50.
   */
  float innerRadius = 50;

  /**
   * The rotation angle in degrees. The default value is 0.
   */
  float rotation = 0;

  /**
   * The roundness of the outer points, ranging from 0 to 100. The default value is 0.
   */
  float outerRoundness = 0;

  /**
   * The roundness of the inner points, ranging from 0 to 100. Only applies when type is
   * Star. The default value is 0.
   */
  float innerRoundness = 0;

  /**
   * Whether the path direction is reversed. The default value is false.
   */
  bool reversed = false;

  NodeType nodeType() const override {
    return NodeType::Polystar;
  }
};

}  // namespace pagx
