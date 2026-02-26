/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

namespace pag {
static constexpr float FLOAT_NEARLY_ZERO = 1.0f / (1 << 12);
static constexpr float Pi = 3.14159265358979323846f;

static inline float DegreesToRadians(float degrees) {
  return degrees * (static_cast<float>(M_PI) / 180.0f);
}

static inline float RadiansToDegrees(float radians) {
  return radians * (180.0f / static_cast<float>(M_PI));
}

static inline bool FloatNearlyZero(float x, float tolerance = FLOAT_NEARLY_ZERO) {
  return fabsf(x) <= tolerance;
}

static inline bool FloatNearlyEqual(float x, float y, float tolerance = FLOAT_NEARLY_ZERO) {
  return fabsf(x - y) <= tolerance;
}

static inline bool DoubleNearlyEqual(double x, double y, double tolerance = FLOAT_NEARLY_ZERO) {
  return fabs(x - y) <= tolerance;
}

static inline bool FloatsAreFinite(const float array[], int count) {
  float prod = 0;
  for (int i = 0; i < count; ++i) {
    prod *= array[i];
  }
  return prod == 0;
}

}  // namespace pag
