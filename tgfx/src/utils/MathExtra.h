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

namespace tgfx {
static constexpr float M_PI_F = static_cast<float>(M_PI);
static constexpr float M_PI_2_F = static_cast<float>(M_PI_2);
static constexpr float FLOAT_NEARLY_ZERO = 1.0f / (1 << 12);
static constexpr float FLOAT_SQRT2 = 1.41421356f;

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

static inline float SinSnapToZero(float radians) {
  float v = sinf(radians);
  return FloatNearlyZero(v) ? 0.0f : v;
}

static inline float CosSnapToZero(float radians) {
  float v = cosf(radians);
  return FloatNearlyZero(v) ? 0.0f : v;
}

static inline bool FloatsAreFinite(const float array[], int count) {
  float prod = 0;
  for (int i = 0; i < count; ++i) {
    prod *= array[i];
  }
  return prod == 0;
}

/**
 * Returns true if value is a power of 2. Does not explicitly check for value <= 0.
 */
template <typename T>
constexpr inline bool IsPow2(T value) {
  return (value & (value - 1)) == 0;
}
}  // namespace tgfx
