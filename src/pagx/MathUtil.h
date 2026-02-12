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

namespace pagx {

inline constexpr float FloatNearlyZero = 1.0f / (1 << 12);
inline constexpr float Pi = 3.14159265358979323846f;

inline bool FloatNearlyEqual(float x, float y) {
  return std::abs(x - y) <= FloatNearlyZero;
}

inline float DegreesToRadians(float degrees) {
  return degrees * Pi / 180.0f;
}

inline float RadiansToDegrees(float radians) {
  return radians * 180.0f / Pi;
}

}  // namespace pagx
