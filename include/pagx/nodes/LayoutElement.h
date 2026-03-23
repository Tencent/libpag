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
 * Group, TextBox, and TextPath. Elements that do not participate in constraint layout (Fill,
 * Stroke, TrimPath, RoundCorner, MergePath, TextModifier, Repeater) inherit directly from
 * Element.
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

 protected:
  LayoutElement() = default;
};

}  // namespace pagx
