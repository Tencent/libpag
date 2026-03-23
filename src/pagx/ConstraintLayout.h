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

#include <utility>
#include <vector>
#include "pagx/layout/ElementMeasure.h"

namespace pagx {

/// Performs constraint positioning on elements within a container, positioning them based on their
/// constraint attributes (left, right, top, bottom, centerX, centerY).
class ConstraintLayout {
 public:
  /// Applies constraint positioning to all elements within a container.
  /// @param elements The elements to position.
  /// @param containerWidth The container width for constraint calculations.
  /// @param containerHeight The container height for constraint calculations.
  /// @param measureText Function to precisely measure Text element bounds.
  static void Apply(const std::vector<Element*>& elements, float containerWidth,
                    float containerHeight, const TextMeasureFunc& measureText);

  /// Returns the content bounds of an element in its local coordinate system.
  /// @param element The element to measure.
  /// @param measureText Function to precisely measure Text element bounds.
  static Rect GetContentBounds(const Element* element, const TextMeasureFunc& measureText);

  /// Returns the measured bounds of an element for bottom-up size calculation, considering
  /// how constraints affect measurement.
  /// @param element The element to measure.
  /// @param measureText Function to precisely measure Text element bounds.
  static Rect GetMeasuredBounds(const Element* element, const TextMeasureFunc& measureText);

  /// Computes the measured content size of a Layer's contents for bottom-up measurement.
  /// @param elements The content elements of the Layer.
  /// @param measureText Function to precisely measure Text element bounds.
  static std::pair<float, float> MeasureContents(const std::vector<Element*>& elements,
                                                 const TextMeasureFunc& measureText);
};

}  // namespace pagx
