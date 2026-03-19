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

#include <functional>
#include "pagx/nodes/Element.h"
#include "pagx/nodes/Text.h"
#include "pagx/types/Rect.h"

namespace pagx {

/**
 * Function type for measuring Text element bounds.
 * Used to inject different text measurement implementations.
 */
using TextMeasureFunc = std::function<Rect(const Text*)>;

/**
 * Utility class for measuring element bounds in layout calculations.
 */
class ElementMeasure {
 public:
  /**
   * Returns the content bounds of an element in its local coordinate system,
   * without applying constraints.
   *
   * For center-anchored elements (Rectangle, Ellipse), the bounds are symmetric
   * around position. For origin-anchored elements (Path, Group), the bounds start
   * from the content's actual coordinates.
   *
   * @param element The element to measure.
   * @param measureText Function to measure Text element bounds.
   */
  static Rect GetContentBounds(const Element* element, const TextMeasureFunc& measureText);

  /**
   * Returns the measured bounds of an element for bottom-up size calculation.
   * Constraints affect measurement:
   * - Opposite-edge constraints (left+right or top+bottom) reset position to 0.
   *   Stretchable types also reset size to 0. Non-stretchable types keep original size.
   * - Single-edge constraints reset position to 0 but keep size.
   * - Center constraints don't affect size.
   *
   * @param element The element to measure.
   * @param measureText Function to measure Text element bounds.
   */
  static Rect GetMeasuredBounds(const Element* element, const TextMeasureFunc& measureText);
};

}  // namespace pagx
