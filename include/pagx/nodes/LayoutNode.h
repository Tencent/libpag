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

#include <cmath>
#include <vector>
#include "pagx/types/Padding.h"
#include "pagx/types/Rect.h"

namespace pagx {

class Element;
class LayoutContext;

/**
 * LayoutNode provides constraint-based positioning for elements within a parent container. It stores
 * constraint attributes (left, right, top, bottom, centerX, centerY) and manages the two-phase
 * layout process: bottom-up measurement followed by top-down constraint resolution.
 */
class LayoutNode {
 public:
  /**
   * Offset in pixels from the left edge of the parent container. NaN means not set.
   */
  float left = NAN;

  /**
   * Offset in pixels from the right edge of the parent container. NaN means not set.
   */
  float right = NAN;

  /**
   * Offset in pixels from the top edge of the parent container. NaN means not set.
   */
  float top = NAN;

  /**
   * Offset in pixels from the bottom edge of the parent container. NaN means not set.
   */
  float bottom = NAN;

  /**
   * Horizontal offset in pixels from the center of the parent container. Positive values shift
   * right. NaN means not set. Takes priority over left/right when set.
   */
  float centerX = NAN;

  /**
   * Vertical offset in pixels from the center of the parent container. Positive values shift
   * down. NaN means not set. Takes priority over top/bottom when set.
   */
  float centerY = NAN;

  virtual ~LayoutNode() = default;

  /** Returns true if any constraint attribute is set. */
  bool hasConstraints() const;

  /**
   * Returns the layout-resolved bounds of this node in its parent's coordinate space.
   * Only valid after applyLayout() has been called. Before layout, returns an empty Rect.
   */
  Rect layoutBounds() const;

  /** Returns the horizontal extent contributed by constraints for parent measurement. */
  float constraintExtentX() const;

  /** Returns the vertical extent contributed by constraints for parent measurement. */
  float constraintExtentY() const;

  /**
   * Phase 1 (bottom-up): measures this node. Container nodes override to recurse children first,
   * then call LayoutNode::updateSize. Leaf nodes use the base implementation directly.
   */
  virtual void updateSize(LayoutContext* context);

  /**
   * Writes self rendering attributes and layoutWidth/layoutHeight. Does not touch children.
   * @param context the current layout context
   * @param width target width from constraints, NaN means use intrinsic size
   * @param height target height from constraints, NaN means use intrinsic size
   */
  virtual void setLayoutSize(LayoutContext* context, float width, float height);

  /** Writes self position and layoutX/layoutY. */
  virtual void setLayoutPosition(LayoutContext* /*context*/, float /*x*/, float /*y*/) {
  }

  /**
   * Lays out children using layoutWidth/layoutHeight as container size.
   * Only container nodes (Group/TextBox/Layer) override this.
   */
  virtual void updateLayout(LayoutContext* /*context*/) {
  }

  /**
   * Constraint layout algorithm: for each node computes target size from constraints,
   * calls setLayoutSize + setLayoutPosition + updateLayout.
   * @param nodes the list of layout nodes to lay out
   * @param containerW the available container width
   * @param containerH the available container height
   * @param padding padding to inset the constraint reference frame
   * @param context the current layout context
   */
  static void PerformConstraintLayout(const std::vector<LayoutNode*>& nodes, float containerW,
                                      float containerH, const Padding& padding,
                                      LayoutContext* context);

  /** Casts an Element to LayoutNode if the element participates in layout. Returns nullptr
   * otherwise. */
  static LayoutNode* AsLayoutNode(Element* element);

  /** Computes position from container size, actual size, and constraints. */
  static Rect CalculateConstrainedPosition(float containerW, float containerH, float layoutW,
                                           float layoutH, const LayoutNode& node);

  /**
   * Computes a uniform scale factor to fit content into target size. Only non-NAN axes contribute.
   * When both axes have targets, returns the smaller scale.
   * @param contentW intrinsic content width
   * @param contentH intrinsic content height
   * @param targetW target width, NaN to ignore this axis
   * @param targetH target height, NaN to ignore this axis
   */
  static float ComputeUniformScale(float contentW, float contentH, float targetW, float targetH);

  /**
   * Collects LayoutNode pointers from a list of elements. Skips elements that don't participate
   * in layout. When skipTextLayout is true, Text and TextBox elements are excluded — used by
   * TextBox::updateLayout to skip its own Text children whose positioning is handled by TextLayout.
   */
  static std::vector<LayoutNode*> CollectLayoutNodes(const std::vector<Element*>& elements,
                                                     bool skipTextLayout);

  /**
   * Shared measurement: computes preferred size from child LayoutNodes.
   * When explicitWidth/explicitHeight is not NAN, that axis uses the explicit value directly.
   * @param elements the child elements to measure
   * @param explicitWidth explicit width override, NaN to compute from children
   * @param explicitHeight explicit height override, NaN to compute from children
   * @param outWidth receives the computed preferred width
   * @param outHeight receives the computed preferred height
   */
  static void MeasureChildNodes(const std::vector<Element*>& elements, float explicitWidth,
                                float explicitHeight, float& outWidth, float& outHeight);

 protected:
  LayoutNode() = default;

  /** Writes measuredWidth/measuredHeight. Called by updateSize when not yet measured. */
  virtual void onMeasure(LayoutContext*) {
  }

 private:
  // Measured position and size (written by onMeasure during updateSize, read-only after that).
  float measuredX = 0;
  float measuredY = 0;
  float measuredWidth = NAN;
  float measuredHeight = NAN;

  // Layout-resolved position and size (written during layout phase, readable via layoutBounds()).
  float layoutX = NAN;
  float layoutY = NAN;
  float layoutWidth = NAN;
  float layoutHeight = NAN;

  friend class Rectangle;
  friend class Ellipse;
  friend class Path;
  friend class Polystar;
  friend class Text;
  friend class TextPath;
  friend class TextBox;
  friend class Group;
  friend class Layer;
};

}  // namespace pagx
