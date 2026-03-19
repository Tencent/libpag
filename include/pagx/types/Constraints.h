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
#include <string>

namespace pagx {

/**
 * Constraints defines the positioning and stretching relationship between a content element and its
 * containing Layer or Group with layout dimensions.
 *
 * Constraint types:
 * - Single-edge (left, right, top, bottom): positions the element without changing its size.
 * - Opposite-edges (left+right or top+bottom): defines a target area. Stretchable elements
 *   (Rectangle, Ellipse, TextBox) and Group fill the area — stretchable types resize their
 *   rendered shape, while Group derives layout dimensions without affecting rendering. Other
 *   types (Polystar, Path, Text) center within the area.
 * - Center (centerX, centerY): positions the element center relative to the container center.
 *   A value of 0 means perfectly centered.
 */
struct Constraints {
  float left = NAN;
  float right = NAN;
  float top = NAN;
  float bottom = NAN;
  float centerX = NAN;
  float centerY = NAN;

  bool hasHorizontal() const {
    return !std::isnan(left) || !std::isnan(right) || !std::isnan(centerX);
  }

  bool hasVertical() const {
    return !std::isnan(top) || !std::isnan(bottom) || !std::isnan(centerY);
  }

  bool hasAny() const {
    return hasHorizontal() || hasVertical();
  }

  /**
   * Validates constraint combinations against the spec rules. Each axis allows either a single
   * edge (left/right/centerX) or opposite edges (left+right). CenterX cannot combine with
   * left or right. Returns false with a diagnostic message when constraints conflict.
   */
  bool isValid(std::string& errorMessage) const {
    if (!std::isnan(centerX) && (!std::isnan(left) || !std::isnan(right))) {
      errorMessage =
          "Conflicting horizontal constraints: centerX cannot be combined with left or right. "
          "Use either one edge (left/right/centerX) or two opposite edges (left+right).";
      return false;
    }
    if (!std::isnan(centerY) && (!std::isnan(top) || !std::isnan(bottom))) {
      errorMessage =
          "Conflicting vertical constraints: centerY cannot be combined with top or bottom. "
          "Use either one edge (top/bottom/centerY) or two opposite edges (top+bottom).";
      return false;
    }
    return true;
  }
};

}  // namespace pagx
