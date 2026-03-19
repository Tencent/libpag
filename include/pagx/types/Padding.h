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

namespace pagx {

/**
 * Padding defines the inner spacing of a layout container. Supports four shorthand forms:
 * - Single value: all four sides equal (e.g., "20")
 * - Two values: vertical, horizontal (e.g., "10,20")
 * - Three values: top, horizontal, bottom (CSS-compatible, e.g., "10,20,10")
 * - Four values: top, right, bottom, left (e.g., "10,20,10,20")
 */
struct Padding {
  float top = 0;
  float right = 0;
  float bottom = 0;
  float left = 0;

  bool isZero() const {
    return top == 0 && right == 0 && bottom == 0 && left == 0;
  }
};

}  // namespace pagx
