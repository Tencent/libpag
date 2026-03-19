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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. See the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "pagx/nodes/Element.h"
#include "pagx/nodes/Group.h"

namespace pagx {

/**
 * Utility class for applying position and size transformations to elements.
 */
class ElementTransform {
 public:
  /**
   * Translates an element horizontally by tx units.
   * Supports all element types that have a position property.
   */
  static void TranslateX(Element* element, float tx);

  /**
   * Translates an element vertically by ty units.
   * Supports all element types that have a position property.
   */
  static void TranslateY(Element* element, float ty);

  /**
   * Applies horizontal stretching to a stretchable element.
   * Only affects Rectangle, Ellipse, and TextBox.
   */
  static void ApplyHorizontalStretch(Element* element, float newWidth, float newCenterX);

  /**
   * Applies vertical stretching to a stretchable element.
   * Only affects Rectangle, Ellipse, and TextBox.
   */
  static void ApplyVerticalStretch(Element* element, float newHeight, float newCenterY);

  /**
   * Applies uniform scaling to non-stretchable elements to fit constraints.
   * - Polystar: scales outerRadius and innerRadius
   * - Path: scales all points in the path data
   * - Text: scales fontSize
   */
  static void ApplyScaling(Element* element, float scale);

  /**
   * Updates a Group's layout size if not already set.
   * Used to derive Group dimensions from their constrained area.
   */
  static void UpdateGroupLayoutSize(Group* group, float derivedWidth, float derivedHeight);
};

}  // namespace pagx
