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
#include <cstdint>

namespace pagx {

/**
 * Represents a 2D point with x and y coordinates.
 */
struct Point {
  float x = 0.0f;
  float y = 0.0f;

  static Point Make(float x, float y) {
    return {x, y};
  }

  static Point Zero() {
    return {0.0f, 0.0f};
  }

  bool operator==(const Point& other) const {
    return x == other.x && y == other.y;
  }

  bool operator!=(const Point& other) const {
    return !(*this == other);
  }

  Point operator+(const Point& other) const {
    return {x + other.x, y + other.y};
  }

  Point operator-(const Point& other) const {
    return {x - other.x, y - other.y};
  }

  Point operator*(float scalar) const {
    return {x * scalar, y * scalar};
  }
};

/**
 * Represents a rectangle with left, top, right, and bottom coordinates.
 */
struct Rect {
  float left = 0.0f;
  float top = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;

  static Rect MakeEmpty() {
    return {0.0f, 0.0f, 0.0f, 0.0f};
  }

  static Rect MakeWH(float width, float height) {
    return {0.0f, 0.0f, width, height};
  }

  static Rect MakeXYWH(float x, float y, float width, float height) {
    return {x, y, x + width, y + height};
  }

  static Rect MakeLTRB(float left, float top, float right, float bottom) {
    return {left, top, right, bottom};
  }

  float x() const {
    return left;
  }

  float y() const {
    return top;
  }

  float width() const {
    return right - left;
  }

  float height() const {
    return bottom - top;
  }

  bool isEmpty() const {
    return left >= right || top >= bottom;
  }

  Point center() const {
    return {(left + right) * 0.5f, (top + bottom) * 0.5f};
  }

  void setEmpty() {
    left = top = right = bottom = 0.0f;
  }

  void join(const Rect& other) {
    if (other.isEmpty()) {
      return;
    }
    if (isEmpty()) {
      *this = other;
      return;
    }
    left = std::min(left, other.left);
    top = std::min(top, other.top);
    right = std::max(right, other.right);
    bottom = std::max(bottom, other.bottom);
  }
};

/**
 * Represents a color with RGBA components in floating point (0.0 to 1.0).
 */
struct Color {
  float red = 0.0f;
  float green = 0.0f;
  float blue = 0.0f;
  float alpha = 1.0f;

  static Color Black() {
    return {0.0f, 0.0f, 0.0f, 1.0f};
  }

  static Color White() {
    return {1.0f, 1.0f, 1.0f, 1.0f};
  }

  static Color Transparent() {
    return {0.0f, 0.0f, 0.0f, 0.0f};
  }

  static Color FromRGBA(float r, float g, float b, float a = 1.0f) {
    return {r, g, b, a};
  }

  /**
   * Creates a color from a 32-bit hex value in AARRGGBB format.
   */
  static Color FromHex(uint32_t hex) {
    return {static_cast<float>((hex >> 16) & 0xFF) / 255.0f,
            static_cast<float>((hex >> 8) & 0xFF) / 255.0f,
            static_cast<float>(hex & 0xFF) / 255.0f,
            static_cast<float>((hex >> 24) & 0xFF) / 255.0f};
  }

  /**
   * Converts the color to a 32-bit hex value in AARRGGBB format.
   */
  uint32_t toHex() const {
    auto r = static_cast<uint32_t>(red * 255.0f + 0.5f);
    auto g = static_cast<uint32_t>(green * 255.0f + 0.5f);
    auto b = static_cast<uint32_t>(blue * 255.0f + 0.5f);
    auto a = static_cast<uint32_t>(alpha * 255.0f + 0.5f);
    return (a << 24) | (r << 16) | (g << 8) | b;
  }

  bool operator==(const Color& other) const {
    return red == other.red && green == other.green && blue == other.blue && alpha == other.alpha;
  }

  bool operator!=(const Color& other) const {
    return !(*this == other);
  }

  Color withAlpha(float newAlpha) const {
    return {red, green, blue, newAlpha};
  }
};

/**
 * Represents a 3x3 transformation matrix for 2D graphics.
 * The matrix is stored in row-major order:
 *   | scaleX  skewX   transX |
 *   | skewY   scaleY  transY |
 *   | 0       0       1      |
 */
struct Matrix {
  float scaleX = 1.0f;
  float skewX = 0.0f;
  float transX = 0.0f;
  float skewY = 0.0f;
  float scaleY = 1.0f;
  float transY = 0.0f;

  static Matrix Identity() {
    return {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
  }

  static Matrix Translate(float tx, float ty) {
    return {1.0f, 0.0f, tx, 0.0f, 1.0f, ty};
  }

  static Matrix Scale(float sx, float sy) {
    return {sx, 0.0f, 0.0f, 0.0f, sy, 0.0f};
  }

  static Matrix Rotate(float degrees) {
    auto radians = degrees * 3.14159265358979323846f / 180.0f;
    auto cosValue = std::cos(radians);
    auto sinValue = std::sin(radians);
    return {cosValue, -sinValue, 0.0f, sinValue, cosValue, 0.0f};
  }

  static Matrix MakeAll(float scaleX, float skewX, float transX, float skewY, float scaleY,
                        float transY) {
    return {scaleX, skewX, transX, skewY, scaleY, transY};
  }

  bool isIdentity() const {
    return scaleX == 1.0f && skewX == 0.0f && transX == 0.0f && skewY == 0.0f && scaleY == 1.0f &&
           transY == 0.0f;
  }

  Matrix operator*(const Matrix& other) const {
    return {scaleX * other.scaleX + skewX * other.skewY,
            scaleX * other.skewX + skewX * other.scaleY,
            scaleX * other.transX + skewX * other.transY + transX,
            skewY * other.scaleX + scaleY * other.skewY,
            skewY * other.skewX + scaleY * other.scaleY,
            skewY * other.transX + scaleY * other.transY + transY};
  }

  Point mapPoint(const Point& point) const {
    return {scaleX * point.x + skewX * point.y + transX,
            skewY * point.x + scaleY * point.y + transY};
  }

  void preTranslate(float tx, float ty) {
    transX += scaleX * tx + skewX * ty;
    transY += skewY * tx + scaleY * ty;
  }

  void preScale(float sx, float sy) {
    scaleX *= sx;
    skewX *= sy;
    skewY *= sx;
    scaleY *= sy;
  }

  void preConcat(const Matrix& other) {
    *this = other * (*this);
  }

  void postConcat(const Matrix& other) {
    *this = (*this) * other;
  }
};

}  // namespace pagx
