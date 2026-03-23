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

#include <memory>
#include <vector>

namespace pagx {

class Element;
class Layer;
class FontConfig;

/**
 * ConstraintLayout is a unified module for applying constraint-based positioning to both Element
 * and Layer objects. It provides a single source of truth for constraint logic, eliminating
 * duplication between element and layer constraint handling.
 *
 * Constraint Positioning Priority (both Element and Layer):
 * - Horizontal: centerX > (left + right) > left > right
 * - Vertical: centerY > (top + bottom) > top > bottom
 */
class ConstraintLayout {
 public:
  /**
   * Applies constraint positioning to an Element within a container of given dimensions.
   * For non-stretchable elements, applies proportional scaling when opposite-edge constraints
   * define a target area.
   *
   * @param element The element to constrain
   * @param containerWidth The width of the container
   * @param containerHeight The height of the container
   * @param fontProvider Font provider for text measurement (can be nullptr)
   * @param skipScaling If true, skips non-stretchable element scaling (for Text in TextBox)
   */
  static void ApplyElementConstraints(Element* element, float containerWidth, float containerHeight,
                                      FontConfig* fontProvider, bool skipScaling = false);

  /**
   * Applies constraint positioning to a Layer within a parent Layer of given dimensions.
   * Handles opposite-edge constraints that modify layer width/height.
   * 
   * Note: For measured child sizes when width/height are NaN, call this overload that
   * provides the measured dimensions explicitly.
   *
   * @param layer The layer to constrain
   * @param parentWidth The width of the parent layer
   * @param parentHeight The height of the parent layer
   * @param measuredWidth The measured width of the layer (used if layer->width is NaN)
   * @param measuredHeight The measured height of the layer (used if layer->height is NaN)
   */
  static void ApplyLayerConstraints(Layer* layer, float parentWidth, float parentHeight,
                                    float measuredWidth, float measuredHeight);

  /**
   * Recursively applies constraint positioning to all elements within a container (Group or
   * Layer). Used internally for processing hierarchical element structures.
   *
   * @param elements The elements to constrain
   * @param containerWidth The width of the container
   * @param containerHeight The height of the container
   * @param fontProvider Font provider for text measurement
   */
  static void ApplyElementsConstraints(const std::vector<Element*>& elements, float containerWidth,
                                       float containerHeight, FontConfig* fontProvider);
};

}  // namespace pagx
