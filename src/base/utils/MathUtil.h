/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
#include <vector>

namespace pag {
static constexpr float FLOAT_NEARLY_ZERO = 1.0f / (1 << 12);

static inline float DegreesToRadians(float degrees) {
  return degrees * (static_cast<float>(M_PI) / 180.0f);
}

static inline float RadiansToDegrees(float radians) {
  return radians * (180.0f / static_cast<float>(M_PI));
}

static inline bool FloatNearlyZero(float x, float tolerance = FLOAT_NEARLY_ZERO) {
  return fabsf(x) <= tolerance;
}

class MathUtil {
 public:
  // 求解一元三次方程式，返回值列表为空，表示方程没有实数解
  static std::vector<double> CalRealSolutionsOfCubicEquation(double a, double b, double c,
                                                             double d);
};

}  // namespace pag
