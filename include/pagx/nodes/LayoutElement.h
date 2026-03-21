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
#include "pagx/nodes/Element.h"

namespace pagx {

/**
 * LayoutElement is the base class for all elements that support constraint-based positioning within
 * a Layer or Group. It provides six constraint attributes (left, right, top, bottom, centerX,
 * centerY) that define how the element is positioned relative to its containing Layer or Group.
 *
 * Elements that inherit from LayoutElement include: Rectangle, Ellipse, Polystar, Path, Text,
 * Group, and TextBox. Elements that do not participate in constraint layout (Fill, Stroke,
 * TrimPath, RoundCorner, MergePath, TextModifier, TextPath, Repeater) inherit directly from
 * Element.
 *
 * Layout Result Storage:
 * During layout computation, LayoutElement stores computed positions and sizes in layoutX, layoutY,
 * layoutWidth, layoutHeight (marked mutable for const context). These are computed properties
 * derived from user-set values plus constraint resolution. Rendering systems should prioritize
 * these layout values when available.
 */
class LayoutElement : public Element {
 public:
  ~LayoutElement() override = default;

  /**
   * Distance from the left edge of the containing Layer or Group. NAN means not set.
   */
  float left = NAN;

  /**
   * Distance from the right edge of the containing Layer or Group. NAN means not set.
   */
  float right = NAN;

  /**
   * Distance from the top edge of the containing Layer or Group. NAN means not set.
   */
  float top = NAN;

  /**
   * Distance from the bottom edge of the containing Layer or Group. NAN means not set.
   */
  float bottom = NAN;

  /**
   * Horizontal offset from the center of the containing Layer or Group. NAN means not set.
   */
  float centerX = NAN;

  /**
   * Vertical offset from the center of the containing Layer or Group. NAN means not set.
   */
  float centerY = NAN;

  /**
   * Applies layout result for size. Called during layout computation to set the computed
   * dimensions. For most elements, this updates position and size; for special types like Group,
   * it may trigger additional layout logic.
   *
   * @param layoutWidth The computed layout width
   * @param layoutHeight The computed layout height
   */
  virtual void setLayoutBoundsSize(float layoutWidth, float layoutHeight) {
    layoutX = 0;
    layoutY = 0;
    layoutWidth_ = layoutWidth;
    layoutHeight_ = layoutHeight;
  }

  /**
   * Applies layout result for position. Called during constraint positioning to update computed
   * position without modifying size.
   *
   * @param layoutX The computed layout X position
   * @param layoutY The computed layout Y position
   */
  virtual void setLayoutBoundsPosition(float layoutX_, float layoutY_) {
    layoutX = layoutX_;
    layoutY = layoutY_;
  }

  /**
   * Computed position X from layout system (mutable allows write from const layout functions).
   */
  mutable float layoutX = 0;

  /**
   * Computed position Y from layout system (mutable allows write from const layout functions).
   */
  mutable float layoutY = 0;

  /**
   * Computed width from layout system (mutable allows write from const layout functions).
   * NAN means not computed.
   */
  mutable float layoutWidth_ = NAN;

  /**
   * Computed height from layout system (mutable allows write from const layout functions).
   * NAN means not computed.
   */
  mutable float layoutHeight_ = NAN;

 protected:
  LayoutElement() = default;
};

}  // namespace pagx
