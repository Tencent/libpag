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

#include "pagx/nodes/LayoutNode.h"
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
   * The center point of the polystar. NaN means not explicitly set, in which case the polystar
   * is positioned so that its top-left corner aligns with the layout origin.
   */
  Point position = {NAN, NAN};

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
   * Unlike using outerRadius as a square, this accounts for the actual vertex positions
   * determined by pointCount, rotation, and innerRadius (for star type).
   */
  Rect getContentBounds() const;

  /**
   * Returns the final center position for rendering, computed from layoutBounds and contentBounds.
   */
  Point renderPosition() const;

  /**
   * Returns the scale factor for rendering, computed from layoutBounds and intrinsic size.
   */
  float renderScale() const;

  /**
   * Returns the outer radius for rendering, accounting for layout scaling.
   */
  float renderOuterRadius() const;

  /**
   * Returns the inner radius for rendering, accounting for layout scaling.
   */
  float renderInnerRadius() const;

 protected:
  void onMeasure(LayoutContext* context) override;
  void setLayoutSize(LayoutContext* context, float width, float height) override;

 private:
  Polystar() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
