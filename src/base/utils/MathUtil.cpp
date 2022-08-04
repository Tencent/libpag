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

#include "MathUtil.h"

namespace pag {
// 范盛金公式求解一元三次方程 a * x^3 + b * x^2 + c * x + d = 0 (a != 0)，获取实数根
// 返回值列表为空，表示方程没有实数解
std::vector<double> MathUtil::CalRealSolutionsOfCubicEquation(double a, double b, double c,
                                                              double d) {
  if (a == 0) {
    return {};
  }

  auto A = b * b - 3 * a * c;
  auto B = b * c - 9 * a * d;
  auto C = c * c - 3 * b * d;
  auto delta = B * B - 4 * A * C;
  std::vector<double> solutions;
  if (A == 0 && B == 0) {
    solutions.emplace_back(-b / (3 * a));
  } else if (delta == 0 && A != 0) {
    auto k = B / A;
    solutions.emplace_back(-b / a + k);
    solutions.emplace_back(-0.5 * k);
  } else if (delta > 0) {
    auto y1 = A * b + 1.5 * a * (-B + sqrt(delta));
    auto y2 = A * b + 1.5 * a * (-B - sqrt(delta));
    solutions.emplace_back((-b - cbrt(y1) - cbrt(y2)) / (3 * a));
  } else if (delta < 0 && A > 0) {
    auto t = (A * b - 1.5 * a * B) / (A * sqrt(A));
    if (-1 < t && t < 1) {
      auto theta = acos(t);
      auto sqrtA = sqrt(A);
      auto cosA = cos(theta / 3);
      auto sinA = sin(theta / 3);
      solutions.emplace_back((-b - 2 * sqrtA * cosA) / (3 * a));
      solutions.emplace_back((-b + sqrtA * (cosA + sqrt(3) * sinA)) / (3 * a));
      solutions.emplace_back((-b + sqrtA * (cosA - sqrt(3) * sinA)) / (3 * a));
    }
  }
  return solutions;
}

}  // namespace pag
