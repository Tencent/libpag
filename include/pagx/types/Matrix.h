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

#include <cmath>
#include "pagx/types/Point.h"

namespace pagx {

/**
 * A 2D affine transformation matrix.
 * Matrix form:
 *   | a  c  tx |
 *   | b  d  ty |
 *   | 0  0  1  |
 */
struct Matrix {
  /**
   * The horizontal scale factor (scaleX).
   */
  float a = 1;

  /**
   * The vertical skew factor (skewY).
   */
  float b = 0;

  /**
   * The horizontal skew factor (skewX).
   */
  float c = 0;

  /**
   * The vertical scale factor (scaleY).
   */
  float d = 1;

  /**
   * The horizontal translation (transX).
   */
  float tx = 0;

  /**
   * The vertical translation (transY).
   */
  float ty = 0;

  /**
   * Returns the identity matrix.
   */
  static Matrix Identity() {
    return {};
  }

  /**
   * Returns a translation matrix.
   */
  static Matrix Translate(float x, float y) {
    Matrix m = {};
    m.tx = x;
    m.ty = y;
    return m;
  }

  /**
   * Returns a scale matrix.
   */
  static Matrix Scale(float sx, float sy) {
    Matrix m = {};
    m.a = sx;
    m.d = sy;
    return m;
  }

  /**
   * Returns a rotation matrix (angle in degrees).
   */
  static Matrix Rotate(float degrees) {
    Matrix m = {};
    float radians = degrees * 3.14159265358979323846f / 180.0f;
    float cosVal = std::cos(radians);
    float sinVal = std::sin(radians);
    m.a = cosVal;
    m.b = sinVal;
    m.c = -sinVal;
    m.d = cosVal;
    return m;
  }

  /**
   * Returns true if this is the identity matrix.
   */
  bool isIdentity() const {
    return a == 1 && b == 0 && c == 0 && d == 1 && tx == 0 && ty == 0;
  }

  /**
   * Concatenates this matrix with another.
   */
  Matrix operator*(const Matrix& other) const {
    Matrix result = {};
    result.a = a * other.a + c * other.b;
    result.b = b * other.a + d * other.b;
    result.c = a * other.c + c * other.d;
    result.d = b * other.c + d * other.d;
    result.tx = a * other.tx + c * other.ty + tx;
    result.ty = b * other.tx + d * other.ty + ty;
    return result;
  }

  /**
   * Transforms a point by this matrix.
   */
  Point mapPoint(const Point& point) const {
    return {a * point.x + c * point.y + tx, b * point.x + d * point.y + ty};
  }

  bool operator==(const Matrix& other) const {
    return a == other.a && b == other.b && c == other.c && d == other.d && tx == other.tx &&
           ty == other.ty;
  }

  bool operator!=(const Matrix& other) const {
    return !(*this == other);
  }
};

}  // namespace pagx
