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

#include <optional>

namespace pagx {

/**
 * Constraints defines the positioning and stretching relationship between a content element and its
 * containing Layer or Group with layout dimensions.
 *
 * Constraint types:
 * - Single-edge (left, right, top, bottom): positions the element without changing its size.
 * - Opposite-edges (left+right or top+bottom): defines a target area. Stretchable elements
 *   (Rectangle, Ellipse, TextBox) fill the area; others center within it.
 * - Center (centerX, centerY): positions the element center relative to the container center.
 *   A value of 0 means perfectly centered.
 */
struct Constraints {
  std::optional<float> left = std::nullopt;
  std::optional<float> right = std::nullopt;
  std::optional<float> top = std::nullopt;
  std::optional<float> bottom = std::nullopt;
  std::optional<float> centerX = std::nullopt;
  std::optional<float> centerY = std::nullopt;

  bool hasHorizontal() const {
    return left.has_value() || right.has_value() || centerX.has_value();
  }

  bool hasVertical() const {
    return top.has_value() || bottom.has_value() || centerY.has_value();
  }

  bool hasAny() const {
    return hasHorizontal() || hasVertical();
  }
};

}  // namespace pagx
