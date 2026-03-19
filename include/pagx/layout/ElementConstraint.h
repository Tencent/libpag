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
#include "pagx/types/Constraints.h"

namespace pagx {

/**
 * Utility class for extracting and querying constraint properties from elements.
 */
class ElementConstraint {
 public:
  /**
   * Checks if an element type can be stretched by layout constraints.
   * Only Rectangle, Ellipse, and TextBox support stretching.
   */
  static bool IsStretchable(NodeType type);

  /**
   * Extracts constraint values from an element.
   * Returns a Constraints struct with left/right/top/bottom/centerX/centerY values.
   * Elements without constraint support return all NAN values.
   */
  static Constraints GetConstraints(const Element* element);
};

}  // namespace pagx
