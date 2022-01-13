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

#include <limits>

namespace pag {
struct Color4f {
  static const Color4f& Invalid() {
    static const auto invalid =
        *new Color4f(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    return invalid;
  }

  static const Color4f& Transparent() {
    static const auto transparent = *new Color4f(0.0f, 0.0f, 0.0f, 0.0f);
    return transparent;
  }

  static Color4f MakeRGB(float r, float g, float b) {
    return {r, g, b, 1.0f};
  }

  Color4f() = default;

  Color4f(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {
  }

  bool operator==(const Color4f& other) const {
    return a == other.a && r == other.r && g == other.g && b == other.b;
  }

  bool operator!=(const Color4f& other) const {
    return !(*this == other);
  }

  /**
   * Returns a pointer to components of Color4f, for array access.
   */
  const float* vec() const {
    return &r;
  }

  /**
   * Returns a pointer to components of Color4f, for array access.
   */
  float* vec() {
    return &r;
  }

  float r = 0;
  float g = 0;
  float b = 0;
  float a = 0;
};
}  // namespace pag
