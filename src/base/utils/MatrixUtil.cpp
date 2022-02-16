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

#include "MatrixUtil.h"

namespace pag {
bool MapPointInverted(const tgfx::Matrix& matrix, tgfx::Point* point) {
  tgfx::Matrix inverted = {};
  bool canInvert = matrix.invert(&inverted);
  if (canInvert) {
    inverted.mapPoints(point, 1);
    return true;
  }
  return false;
}

float GetMaxScaleFactor(const tgfx::Matrix& matrix) {
  auto scale = GetScaleFactor(matrix);
  return std::max(fabsf(scale.x), fabsf(scale.y));
}

tgfx::Point GetScaleFactor(const tgfx::Matrix& matrix, float contentScale, bool inverted) {
  tgfx::Point scale = {};
  auto a = matrix.get(0);
  auto c = matrix.get(1);
  auto b = matrix.get(3);
  auto d = matrix.get(4);
  float determinant = a * d - b * c;
  if (a == 1 && b == 0) {
    scale.x = 1;
  } else {
    auto result = sqrtf(a * a + b * b);
    scale.x = determinant < 0 ? -result : result;
  }
  if (c == 0 && d == 1) {
    scale.y = 1;
  } else {
    auto result = sqrtf(c * c + d * d);
    scale.y = determinant < 0 ? -result : result;
  }
  scale.x *= contentScale;
  scale.y *= contentScale;

  if (inverted) {
    scale.x = scale.x == 0 ? 0 : 1 / scale.x;
    scale.y = scale.y == 0 ? 0 : 1 / scale.y;
  }
  return scale;
}

}  // namespace pag