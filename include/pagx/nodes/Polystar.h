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
#include <cmath>
#include <limits>
#include "pagx/layout/LayoutNode.h"
#include "pagx/nodes/Element.h"
#include "pagx/types/Point.h"
#include "pagx/types/PolystarType.h"
#include "pagx/types/Rect.h"

namespace pagx {

/**
 * Polystar represents a polygon or star shape with configurable points, radii, and roundness.
 */
class Polystar : public Element, public LayoutNode {
 public:
  /**
   * The center point of the polystar. When not explicitly set, defaults to
   * (outerRadius, outerRadius) so that the top-left corner aligns with the origin (0, 0).
   */
  Point position = {};

  /**
   * The type of polystar shape, either Star or Polygon. The default value is Star.
   */
  PolystarType type = PolystarType::Star;

  /**
   * The number of points in the polystar. The default value is 5.
   */
  float pointCount = 5.0f;

  /**
   * The outer radius of the polystar. The default value is 100.
   */
  float outerRadius = 100.0f;

  /**
   * The inner radius of the polystar. Only applies when type is Star. The default value is 50.
   */
  float innerRadius = 50.0f;

  /**
   * The rotation angle in degrees. The default value is 0.
   */
  float rotation = 0.0f;

  /**
   * The roundness of the outer points, ranging from 0 to 1. The default value is 0.
   */
  float outerRoundness = 0.0f;

  /**
   * The roundness of the inner points, ranging from 0 to 1. Only applies when type is Star. The
   * default value is 0.
   */
  float innerRoundness = 0.0f;

  /**
   * Whether the path direction is reversed. The default value is false.
   */
  bool reversed = false;

  NodeType nodeType() const override {
    return NodeType::Polystar;
  }

  /**
   * Computes the tight bounding box of the polystar by iterating over all vertices.
   * This is used for rendering only — layout uses the frame-aligned bounds
   * (outerRadius × 2, outerRadius × 2) instead.
   */
  Rect computeBounds() const {
    auto numPoints = static_cast<int>(ceilf(pointCount));
    float startAngle = (rotation - 90.0f) * static_cast<float>(M_PI) / 180.0f;
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    if (type == PolystarType::Star) {
      float angleStep = static_cast<float>(M_PI) / pointCount;
      float angle = startAngle;
      for (int i = 0; i < numPoints * 2; i++) {
        float radius = (i % 2 == 0) ? outerRadius : innerRadius;
        float vx = radius * cosf(angle);
        float vy = radius * sinf(angle);
        minX = std::min(minX, vx);
        minY = std::min(minY, vy);
        maxX = std::max(maxX, vx);
        maxY = std::max(maxY, vy);
        angle += angleStep;
      }
    } else {
      float angleStep = 2.0f * static_cast<float>(M_PI) / static_cast<float>(numPoints);
      float angle = startAngle;
      for (int i = 0; i < numPoints; i++) {
        float vx = outerRadius * cosf(angle);
        float vy = outerRadius * sinf(angle);
        minX = std::min(minX, vx);
        minY = std::min(minY, vy);
        maxX = std::max(maxX, vx);
        maxY = std::max(maxY, vy);
        angle += angleStep;
      }
    }
    return Rect::MakeXYWH(minX, minY, maxX - minX, maxY - minY);
  }

 protected:
  void onMeasure(const LayoutContext& context) override;
  void setLayoutSize(const LayoutContext& context, float width, float height) override;
  void setLayoutPosition(const LayoutContext& context, float x, float y) override;

 private:
  Polystar() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
