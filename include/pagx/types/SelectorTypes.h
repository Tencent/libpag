/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 THL A29 Limited, a Tencent company. All rights reserved.
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
 * The unit type for range selector values.
 */
enum class SelectorUnit {
  /**
   * Values are specified as character indices.
   */
  Index,
  /**
   * Values are specified as percentages of the total text length.
   */
  Percentage
};

/**
 * The shape of the selection falloff curve.
 */
enum class SelectorShape {
  /**
   * A square falloff with no transition.
   */
  Square,
  /**
   * A ramp that increases from start to end.
   */
  RampUp,
  /**
   * A ramp that decreases from start to end.
   */
  RampDown,
  /**
   * A triangle shape that peaks in the middle.
   */
  Triangle,
  /**
   * A rounded falloff with smooth edges.
   */
  Round,
  /**
   * A smooth S-curve falloff.
   */
  Smooth
};

/**
 * The mode for combining multiple selectors.
 */
enum class SelectorMode {
  /**
   * Add selector values together.
   */
  Add,
  /**
   * Subtract selector values.
   */
  Subtract,
  /**
   * Use the intersection of selector ranges.
   */
  Intersect,
  /**
   * Use the minimum of selector values.
   */
  Min,
  /**
   * Use the maximum of selector values.
   */
  Max,
  /**
   * Use the absolute difference of selector values.
   */
  Difference
};

}  // namespace pagx
