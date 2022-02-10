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

/**
 * RGBA color value, holding four floating point components. Color components are always in a known
 * order.
 */
struct Color4f {
  /**
   * Red component.
   */
  float r;

  /**
   * Green component.
   */
  float g;

  /**
   * Blue component.
   */
  float b;

  /**
   * Alpha component.
   */
  float a;

  /**
   * Returns an invalid Color.
   */
  static const Color4f& Invalid() {
    static const Color4f color = {
        std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    return color;
  }

  /**
   * Returns a fully transparent Color.
   */
  static const Color4f& Transparent() {
    static const Color4f color = {0.0f, 0.0f, 0.0f, 0.0f};
    return color;
  }

  /**
   * Returns a fully opaque black Color.
   */
  static const Color4f& Black() {
    static const Color4f color = {0.0f, 0.0f, 0.0f, 1.0f};
    return color;
  }

  /**
   * Returns a fully opaque white Color.
   */
  static const Color4f& White() {
    static const Color4f color = {1.0f, 1.0f, 1.0f, 1.0f};
    return color;
  }

  /**
   * Compares Color with other, and returns true if all components are equal.
   */
  bool operator==(const Color4f& other) const {
    return a == other.a && r == other.r && g == other.g && b == other.b;
  }

  /**
   * Compares Color with other, and returns true if not all components are equal.
   */
  bool operator!=(const Color4f& other) const {
    return !(*this == other);
  }

  /**
   * Returns a pointer to components of Color, for array access.
   */
  const float* vector() const {
    return &r;
  }

  /**
   * Returns a pointer to components of Color, for array access.
   */
  float* vector() {
    return &r;
  }

  /**
   * Returns true if all channels are in [0, 1].
   **/
  bool isValid() const {
    return r >= 0.0f && r <= 1.0f && g >= 0.0f && g <= 1.0f && b >= 0.0f && b <= 1.0f &&
           a >= 0.0f && a <= 1.0f;
  }

  /**
   * Returns true if Color is an opaque color.
   */
  bool isOpaque() const {
    return a == 1.0f;
  }

  /**
   * Returns a Color with alpha set to 1.0.
   */
  Color4f makeOpaque() const {
    return {r, g, b, 1.0f};
  }

  /**
   * Returns a Color premultiplied by alpha.
   */
  Color4f premultiply() const {
    return {r * a, g * a, b * a, a};
  }

  /**
   * Returns a Color unpremultiplied by alpha.
   */
  Color4f unpremultiply() const;
};
}  // namespace pag
