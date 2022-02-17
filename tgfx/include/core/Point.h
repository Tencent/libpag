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
#include <memory>
#include <string>
#include <vector>

namespace tgfx {
/**
 * Point holds two 32-bit floating point coordinates.
 */
struct Point {
  /**
   * x-axis value.
   */
  float x;
  /**
   * y-axis value.
   */
  float y;

  /**
   * Creates a Point set to (0, 0).
   */
  static const Point& Zero() {
    static const Point zero = Point::Make(0, 0);
    return zero;
  }

  /**
   * Creates a Point with specified x and y value.
   */
  static constexpr Point Make(float x, float y) {
    return {x, y};
  }

  /**
   * Returns true if fX and fY are both zero.
   */
  bool isZero() const {
    return (0 == x) & (0 == y);
  }

  /**
   * Sets fX to x and fY to y.
   */
  void set(float xValue, float yValue) {
    x = xValue;
    y = yValue;
  }

  /**
   * Adds offset (dx, dy) to Point.
   */
  void offset(float dx, float dy) {
    x += dx;
    y += dy;
  }

  /**
   * Returns the Euclidean distance from origin.
   */
  float length() const {
    return Point::Length(x, y);
  }

  /**
   * Returns true if a is equivalent to b.
   */
  friend bool operator==(const Point& a, const Point& b) {
    return a.x == b.x && a.y == b.y;
  }

  /**
   * Returns true if a is not equivalent to b.
   */
  friend bool operator!=(const Point& a, const Point& b) {
    return a.x != b.x || a.y != b.y;
  }

  /**
   * Returns a Point from b to a; computed as (a.fX - b.fX, a.fY - b.fY).
   */
  friend Point operator-(const Point& a, const Point& b) {
    return {a.x - b.x, a.y - b.y};
  }

  /**
   * Returns Point resulting from Point a offset by Point b, computed as:
   * (a.fX + b.fX, a.fY + b.fY).
   */
  friend Point operator+(const Point& a, const Point& b) {
    return {a.x + b.x, a.y + b.y};
  }

  /**
   * Returns the Euclidean distance from origin.
   */
  static float Length(float x, float y) {
    return sqrt(x * x + y * y);
  }

  /**
   * Returns the Euclidean distance between a and b.
   */
  static float Distance(const Point& a, const Point& b) {
    return Length(a.x - b.x, a.y - b.y);
  }
};
}  // namespace tgfx
