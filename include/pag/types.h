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

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "pag/defines.h"

namespace pag {

typedef uint32_t ID;
typedef float Percent;    // in the range [0 - 1]
typedef uint8_t Opacity;  // in the range [0 - 255]
typedef int64_t Frame;

static constexpr ID ZeroID = 0;
static constexpr Frame ZeroFrame = 0;
static constexpr Opacity Opaque = 255;
static constexpr Opacity Transparent = 0;

/**
 * Concatenates two opacity values.
 */
inline Opacity PAG_API OpacityConcat(Opacity a, Opacity b) {
  return static_cast<Opacity>(a * b / 255);
}

struct RTTR_AUTO_REGISTER_CLASS Color {
  uint8_t red, green, blue;  // in the range [0 - 255]
};

inline bool operator==(const Color& left, const Color& right) {
  return left.red == right.red && left.green == right.green && left.blue == right.blue;
}

inline bool operator!=(const Color& left, const Color& right) {
  return !(left == right);
}

static constexpr Color Black = {0, 0, 0};
static constexpr Color White = {255, 255, 255};
static constexpr Color Red = {255, 0, 0};
static constexpr Color Green = {0, 255, 0};
static constexpr Color Blue = {0, 0, 255};

/**
 * TimeRange represents a range of frames.
 */
struct RTTR_AUTO_REGISTER_CLASS TimeRange {
  /**
   * The start frame of the time range, which is included.
   */
  Frame start;
  /**
   * The end frame of the time range, which is also included.
   */
  Frame end;

  Frame duration() const {
    return end - start + 1;
  }

  bool isValid() const {
    return 0 <= start && start <= end;
  }

  bool contains(Frame frame) const {
    return start <= frame && frame <= end;
  }
};

/**
 * Point holds two 32-bit floating point coordinates.
 */
struct PAG_API Point {
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
     * Creates a Point with specified x and y value.
     */
  static constexpr Point Make(int x, int y) {
    return {static_cast<float>(x), static_cast<float>(y)};
  }

  /**
   * Returns true if x and y are both zero.
   */
  bool isZero() const {
    return (0 == x) & (0 == y);
  }

  /**
   * Sets xValue to x and yValue to y.
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

/**
 * Point holds three 32-bit floating point coordinates.
 */
struct PAG_API Point3D {
  /**
   * x-axis value.
   */
  float x;
  /**
   * y-axis value.
   */
  float y;
  /**
   * z-axis value.
   */
  float z;

  /**
   * Creates a Point set to (0, 0, 0).
   */
  static const Point3D& Zero() {
    static const Point3D zero = Point3D::Make(0, 0, 0);
    return zero;
  }

  /**
   * Creates a Point with specified x y z value.
   */
  static constexpr Point3D Make(float x, float y, float z) {
    return {x, y, z};
  }

  /**
   * Returns true if x y z are all zero.
   */
  bool isZero() const {
    return (0 == x) & (0 == y) & (0 == z);
  }

  /**
   * Sets xValue to x, yValue to y and zValue to z.
   */
  void set(float xValue, float yValue, float zValue) {
    x = xValue;
    y = yValue;
    z = zValue;
  }

  /**
   * Adds offset (dx, dy, dz) to Point.
   */
  void offset(float dx, float dy, float dz) {
    x += dx;
    y += dy;
    z += dz;
  }

  /**
   * Returns the Euclidean distance from origin.
   */
  float length() const {
    return Point3D::Length(x, y, z);
  }

  /**
   * Returns true if a is equivalent to b.
   */
  friend bool operator==(const Point3D& a, const Point3D& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
  }

  /**
   * Returns true if a is not equivalent to b.
   */
  friend bool operator!=(const Point3D& a, const Point3D& b) {
    return a.x != b.x || a.y != b.y || a.z != b.z;
  }

  /**
   * Returns a Point from b to a; computed as (a.fX - b.fX, a.fY - b.fY, a.fZ - b.fZ).
   */
  friend Point3D operator-(const Point3D& a, const Point3D& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
  }

  /**
   * Returns Point resulting from Point a offset by Point b, computed as:
   * (a.x + b.x, a.y + b.y, a.z + b.z).
   */
  friend Point3D operator+(const Point3D& a, const Point3D& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
  }

  /**
   * Returns the Euclidean distance from origin.
   */
  static float Length(float x, float y, float z) {
    return sqrt(x * x + y * y + z * z);
  }

  /**
   * Returns the Euclidean distance between a and b.
   */
  static float Distance(const Point3D& a, const Point3D& b) {
    return Length(a.x - b.x, a.y - b.y, a.z - b.z);
  }
};

/**
 * Rect holds four float coordinates describing the upper and lower bounds of a rectangle. Rect may
 * be created from outer bounds or from position, width, and height. Rect describes an area; if its
 * right is less than or equal to its left, or if its bottom is less than or equal to its top, it is
 * considered empty.
 */
struct PAG_API Rect {
  /**
   * smaller x-axis bounds.
   */
  float left;
  /**
   * smaller y-axis bounds.
   */
  float top;
  /**
   * larger x-axis bounds.
   */
  float right;
  /**
   * larger y-axis bounds.
   */
  float bottom;

  /**
   * Returns constructed Rect set to (0, 0, 0, 0).
   */
  static constexpr Rect MakeEmpty() {
    return {0, 0, 0, 0};
  }

  /**
   * Returns constructed Rect set to float values (0, 0, w, h). Does not validate input; w or h may
   * be negative.
   */
  static constexpr Rect MakeWH(float w, float h) {
    return {0, 0, w, h};
  }

  /**
   * Returns constructed Rect set to float values (0, 0, w, h). Does not validate input; w or h may
   * be negative.
   */
  static constexpr Rect MakeWH(int w, int h) {
    return {0, 0, static_cast<float>(w), static_cast<float>(h)};
  }

  /**
   * Returns constructed Rect set to (l, t, r, b). Does not sort input; Rect may result in left
   * greater than right, or top greater than bottom.
   */
  static constexpr Rect MakeLTRB(float l, float t, float r, float b) {
    return {l, t, r, b};
  }

  /**
   * Returns constructed Rect set to (x, y, x + w, y + h). Does not validate input; w or h may be
   * negative.
   */
  static constexpr Rect MakeXYWH(float x, float y, float w, float h) {
    return {x, y, x + w, y + h};
  }

  /**
   * Returns true if left is equal to or greater than right, or if top is equal to or greater
   * than bottom. Call sort() to reverse rectangles with negative width() or height().
   */
  bool isEmpty() const {
    // We write it as the NOT of a non-empty rect, so we will return true if any values
    // are NaN.
    return !(left < right && top < bottom);
  }

  /**
   * Returns true if left is equal to or less than right, or if top is equal to or less than
   * bottom. Call sort() to reverse rectangles with negative width() or height().
   */
  bool isSorted() const {
    return left <= right && top <= bottom;
  }

  /**
   * Returns left edge of Rect, if sorted. Call isSorted() to see if Rect is valid. Call sort() to
   * reverse left and right if needed.
   */
  float x() const {
    return left;
  }

  /**
   * Returns top edge of Rect, if sorted. Call isEmpty() to see if Rect may be invalid, and sort()
   * to reverse top and bottom if needed.
   */
  float y() const {
    return top;
  }

  /**
   * Returns span on the x-axis. This does not check if Rect is sorted, or if result fits in 32-bit
   * float; result may be negative or infinity.
   */
  float width() const {
    return right - left;
  }

  /**
   * Returns span on the y-axis. This does not check if Rect is sorted, or if result fits in 32-bit
   * float; result may be negative or infinity.
   */
  float height() const {
    return bottom - top;
  }

  /**
   * Returns average of left edge and right edge. Result does not change if Rect is sorted.
   */
  float centerX() const {
    // don't use (left + bottom) * 0.5f as that might overflow before the 0.5f
    return left * 0.5f + right * 0.5f;
  }

  /**
   * Returns average of top edge and bottom edge. Result does not change if Rect is sorted.
   */
  float centerY() const {
    // don't use (top + bottom) * 0.5f as that might overflow before the 0.5f
    return top * 0.5f + bottom * 0.5f;
  }

  /**
   * Returns true if all members in a: left, top, right, and bottom; are equal to the
   * corresponding members in b.
   * a and b are not equal if either contain NaN. a and b are equal if members contain zeroes with
   * different signs.
   */
  friend bool operator==(const Rect& a, const Rect& b) {
    return a.left == b.left && a.right == b.right && a.top == b.top && a.bottom == b.bottom;
  }

  /**
   * Returns true if any in a: left, top, right, and bottom; does not equal the corresponding
   * members in b.
   * a and b are not equal if either contain NaN. a and b are equal if members contain zeroes with
   * different signs.
   */
  friend bool operator!=(const Rect& a, const Rect& b) {
    return a.left != b.left || a.right != b.right || a.top != b.top || a.bottom != b.bottom;
  }

  /**
   * Sets Rect to (0, 0, 0, 0).
   */
  void setEmpty() {
    *this = MakeEmpty();
  }

  /**
   * Sets Rect to (left, top, right, bottom). left and right are not sorted; left is not necessarily
   * less than right. top and bottom are not sorted; top is not necessarily less than bottom.
   */
  void setLTRB(float l, float t, float r, float b) {
    left = l;
    top = t;
    right = r;
    bottom = b;
  }

  /**
   * Sets to bounds of Point array with count entries. Returns false if count is zero or smaller, or
   * if Point array contains an infinity or NaN; in these cases sets Rect to (0, 0, 0, 0).
   * Result is either empty or sorted: left is less than or equal to right, and top is less than
   * or equal to bottom.
   */
  bool setBounds(const Point pts[], int count);

  /**
   * Sets Rect to (x, y, x + width, y + height). Does not validate input; width or height may be
   * negative.
   */
  void setXYWH(float x, float y, float width, float height) {
    left = x;
    top = y;
    right = x + width;
    bottom = y + height;
  }

  /**
   * Sets Rect to (0, 0, width, height). Does not validate input, width or height may be negative.
   */
  void setWH(float width, float height) {
    left = 0;
    top = 0;
    right = width;
    bottom = height;
  }

  /**
   * Returns Rect offset by (dx, dy).
   * If dx is negative, Rect returned is moved to the left.
   * If dx is positive, Rect returned is moved to the right.
   * If dy is negative, Rect returned is moved upward.
   * If dy is positive, Rect returned is moved downward.
   */
  Rect makeOffset(float dx, float dy) const {
    return MakeLTRB(left + dx, top + dy, right + dx, bottom + dy);
  }

  /**
   * Returns Rect, inset by (dx, dy).
   * If dx is negative, Rect returned is wider.
   * If dx is positive, Rect returned is narrower.
   * If dy is negative, Rect returned is taller.
   * If dy is positive, Rect returned is shorter.
   */
  Rect makeInset(float dx, float dy) const {
    return MakeLTRB(left + dx, top + dy, right - dx, bottom - dy);
  }

  /**
   * Returns Rect, outset by (dx, dy).
   * If dx is negative, Rect returned is narrower.
   * If dx is positive, Rect returned is wider.
   * If dy is negative, Rect returned is shorter.
   * If dy is positive, Rect returned is taller.
   */
  Rect makeOutset(float dx, float dy) const {
    return MakeLTRB(left - dx, top - dy, right + dx, bottom + dy);
  }

  /**
   * Offsets Rect by adding dx to left, right; and by adding dy to top, bottom.
   * If dx is negative, moves Rect to the left.
   * If dx is positive, moves Rect to the right.
   * If dy is negative, moves Rect upward.
   * If dy is positive, moves Rect downward.
   */
  void offset(float dx, float dy) {
    left += dx;
    top += dy;
    right += dx;
    bottom += dy;
  }

  /**
   * Offsets Rect by adding delta.fX to left, right; and by adding delta.y to top, bottom.
   * If delta.x is negative, moves Rect to the left.
   * If delta.x is positive, moves Rect to the right.
   * If delta.y is negative, moves Rect upward.
   * If delta.y is positive, moves Rect downward.
   */
  void offset(const Point& delta) {
    this->offset(delta.x, delta.y);
  }

  /**
   * Offsets Rect so that left equals newX, and top equals newY. width and height are unchanged.
   */
  void offsetTo(float newX, float newY) {
    right += newX - left;
    bottom += newY - top;
    left = newX;
    top = newY;
  }

  /**
   * Insets Rect by (dx, dy).
   * If dx is positive, makes Rect narrower.
   * If dx is negative, makes Rect wider.
   * If dy is positive, makes Rect shorter.
   * If dy is negative, makes Rect taller.
   */
  void inset(float dx, float dy) {
    left += dx;
    top += dy;
    right -= dx;
    bottom -= dy;
  }

  /**
   * Outsets Rect by (dx, dy).
   * If dx is positive, makes Rect wider.
   * If dx is negative, makes Rect narrower.
   * If dy is positive, makes Rect taller.
   * If dy is negative, makes Rect shorter.
   */
  void outset(float dx, float dy) {
    this->inset(-dx, -dy);
  }

  /**
   * Scale the rectangle by scaleX and scaleY.
   */
  void scale(float scaleX, float scaleY);

  /**
   * Returns true if Rect intersects r, and sets Rect to intersection. Returns false if Rect does
   * not intersect r, and leaves Rect unchanged. Returns false if either r or Rect is empty, leaving
   * Rect unchanged.
   */
  bool intersect(const Rect& r) {
    return this->intersect(r.left, r.top, r.right, r.bottom);
  }

  /**
   * Constructs Rect to intersect from (left, top, right, bottom). Does not sort construction.
   * Returns true if Rect intersects construction, and sets Rect to intersection.
   * Returns false if Rect does not intersect construction, and leaves Rect unchanged.
   * Returns false if either construction or Rect is empty, leaving Rect unchanged.
   */
  bool intersect(float l, float t, float r, float b);

  /**
   * Returns true if a intersects b, and sets Rect to intersection.
   * Returns false if a does not intersect b, and leaves Rect unchanged.
   * Returns false if either a or b is empty, leaving Rect unchanged.
   */
  bool intersect(const Rect& a, const Rect& b);

  /**
   * Constructs Rect to intersect from (left, top, right, bottom). Does not sort construction.
   * Returns true if Rect intersects construction.
   * Returns false if either construction or Rect is empty, or do not intersect.
   */
  bool intersects(float l, float t, float r, float b) const;

  /**
   * Returns true if Rect intersects r. Returns false if either r or Rect is empty, or do not
   * intersect.
   */
  bool intersects(const Rect& r) const;

  /**
   * Returns true if a intersects b. Returns false if either a or b is empty, or do not intersect.
   */
  static bool Intersects(const Rect& a, const Rect& b);

  /**
   * Constructs Rect to intersect from (left, top, right, bottom). Does not sort construction.
   * Sets Rect to the union of itself and the construction. Has no effect if construction is empty.
   * Otherwise, if Rect is empty, sets Rect to construction.
   */
  void join(float l, float t, float r, float b);

  /**
   * Sets Rect to the union of itself and r. Has no effect if r is empty. Otherwise, if Rect is
   * empty, sets Rect to r.
   */
  void join(const Rect& r) {
    this->join(r.left, r.top, r.right, r.bottom);
  }

  /**
   * Returns true if: left <= x < right && top <= y < bottom. Returns false if Rect is empty.
   */
  bool contains(float x, float y) const {
    return x >= left && x < right && y >= top && y < bottom;
  }

  /**
   * Returns true if Rect contains r. Returns false if Rect is empty or r is empty. Rect contains r
   * when Rect area completely includes r area.
   */
  bool contains(const Rect& r) const {
    return left <= r.left && top <= r.top && right >= r.right && bottom >= r.bottom;
  }

  /**
   * Sets Rect by discarding the fractional portion of left and top; and rounding up right and
   * bottom.
   */
  void roundOut();

  /**
   * Sets Rect by rounding of left, top,  right and bottom.
   */
  void round();

  /**
   * Swaps left and right if left is greater than right; and swaps top and bottom if top is
   * greater than bottom. Result may be empty; and width() and height() will be zero or positive.
   */
  void sort();

  /**
   * Returns Rect with left and right swapped if left is greater than right, and with top and
   * bottom swapped if top is greater than bottom. Result may be empty, and width() and height()
   * will be zero or positive.
   */
  Rect makeSorted() const;
};

/***
 * Matrix holds a 3x3 matrix for transforming coordinates. This allows mapping Point and vectors
 * with translation, scaling, skewing, rotation, and perspective.
 * Matrix elements are in row major order. Matrix does not have a constructor, so it must be
 * explicitly initialized. setIdentity() initializes Matrix so it has no effect.
 * setTranslate(), setScale(), setSkew(), setRotate(), set9 and setAll() initializes all Matrix
 * elements with the corresponding mapping.
 */
class PAG_API Matrix {
 public:
  /**
   * Sets Matrix to scale by (sx, sy). Returned matrix is:
   *
   *       | sx  0  0 |
   *       |  0 sy  0 |
   *       |  0  0  1 |
   *
   *  @param sx  horizontal scale factor
   *  @param sy  vertical scale factor
   *  @return    Matrix with the scale factors.
   */
  static Matrix MakeScale(float sx, float sy) {
    Matrix m = {};
    m.setScale(sx, sy);
    return m;
  }

  /**
   * Sets Matrix to scale by (scale, scale). Returned matrix is:
   *
   *      | scale   0   0 |
   *      |   0   scale 0 |
   *      |   0     0   1 |
   *
   * @param scale  horizontal and vertical scale factor
   * @return       Matrix with the scale factors.
   */
  static Matrix MakeScale(float scale) {
    Matrix m = {};
    m.setScale(scale, scale);
    return m;
  }

  /**
   * Sets Matrix to translate by (dx, dy). Returned matrix is:
   *
   *       | 1 0 dx |
   *       | 0 1 dy |
   *       | 0 0  1 |
   *
   * @param dx  horizontal translation
   * @param dy  vertical translation
   * @return    Matrix with translation
   */
  static Matrix MakeTrans(float dx, float dy) {
    Matrix m = {};
    m.setTranslate(dx, dy);
    return m;
  }

  /**
   * Sets Matrix to:
   *
   *      | scaleX  skewX transX |
   *      |  skewY scaleY transY |
   *      |  pers0  pers1  pers2 |
   *
   * @param scaleX  horizontal scale factor
   * @param skewX   horizontal skew factor
   * @param transX  horizontal translation
   * @param skewY   vertical skew factor
   * @param scaleY  vertical scale factor
   * @param transY  vertical translation
   * @param pers0   input x-axis perspective factor
   * @param pers1   input y-axis perspective factor
   * @param pers2   perspective scale factor
   * @return        Matrix constructed from parameters
   */
  static Matrix MakeAll(float scaleX, float skewX, float transX, float skewY, float scaleY,
                        float transY, float pers0, float pers1, float pers2) {
    Matrix m = {};
    m.setAll(scaleX, skewX, transX, skewY, scaleY, transY, pers0, pers1, pers2);
    return m;
  }

  /**
   * Returns true if Matrix is identity. The identity matrix is:
   *
   *       | 1 0 0 |
   *       | 0 1 0 |
   *       | 0 0 1 |
   *
   * @return  Returns true if the Matrix has no effect
   */
  bool isIdentity() const;

  /**
   * Returns one matrix value.
   */
  float operator[](int index) const;

  /**
   * Returns one matrix value.
   */
  float get(int index) const;

  /**
   * Returns the scale factor multiplied by x-axis input, contributing to x-axis output. With
   * mapPoints(), scales Point along the x-axis.
   * @return  horizontal scale factor
   */
  float getScaleX() const {
    return values[SCALE_X];
  }

  /**
   * Returns the scale factor multiplied by y-axis input, contributing to y-axis output. With
   * mapPoints(), scales Point along the y-axis.
   * @return  vertical scale factor
   */
  float getScaleY() const {
    return values[SCALE_Y];
  }

  /**
   * Returns the scale factor multiplied by x-axis input, contributing to y-axis output. With
   * mapPoints(), skews Point along the y-axis. Skewing both axes can rotate Point.
   * @return  vertical skew factor
   */
  float getSkewY() const {
    return values[SKEW_Y];
  }

  /**
   * Returns the scale factor multiplied by y-axis input, contributing to x-axis output. With
   * mapPoints(), skews Point along the x-axis. Skewing both axes can rotate Point.
   * @return  horizontal scale factor
   */
  float getSkewX() const {
    return values[SKEW_X];
  }

  /**
   * Returns translation contributing to x-axis output. With mapPoints(), moves Point along the
   * x-axis.
   * @return  horizontal translation factor
   */
  float getTranslateX() const {
    return values[TRANS_X];
  }

  /**
   * Returns translation contributing to y-axis output. With mapPoints(), moves Point along the
   * y-axis.
   * @return  vertical translation factor
   */
  float getTranslateY() const {
    return values[TRANS_Y];
  }

  /**
   * Returns writable Matrix value.
   */
  float& operator[](int index);

  /**
   * Sets Matrix value.
   */
  void set(int index, float value);

  /**
   * Sets horizontal scale factor.
   * @param v  horizontal scale factor to store
   */
  void setScaleX(float v);

  /**
   * Sets the vertical scale factor.
   * @param v  vertical scale factor to store
   */
  void setScaleY(float v);

  /**
   * Sets vertical skew factor.
   * @param v  vertical skew factor to store
   */
  void setSkewY(float v);

  /**
   * Sets the horizontal skew factor.
   * @param v  horizontal skew factor to store
   */
  void setSkewX(float v);

  /**
   * Sets horizontal translation.
   * @param v  horizontal translation to store
   */
  void setTranslateX(float v);

  /**
   * Sets vertical translation.
   * @param v  vertical translation to store
   */
  void setTranslateY(float v);

  /**
   * Sets all values from parameters. Sets matrix to:
   *
   *      | scaleX  skewX transX |
   *      |  skewY scaleY transY |
   *      | persp0 persp1 persp2 |
   *
   * @param scaleX  horizontal scale factor to store
   * @param skewX   horizontal skew factor to store
   * @param transX  horizontal translation to store
   * @param skewY   vertical skew factor to store
   * @param scaleY  vertical scale factor to store
   * @param transY  vertical translation to store
   * @param persp0  input x-axis values perspective factor to store
   * @param persp1  input y-axis values perspective factor to store
   * @param persp2  perspective scale factor to store
   */
  void setAll(float scaleX, float skewX, float transX, float skewY, float scaleY, float transY,
              float persp0, float persp1, float persp2);

  /**
   * Sets the Matrix to affine values, passed in column major order:
   *
   *      | a c tx |
   *      | b d ty |
   *      | 0    1 |
   *
   * @param a  horizontal scale factor
   * @param b  vertical skew factor
   * @param c  horizontal skew factor
   * @param d  vertical scale factor
   * @param tx horizontal translation
   * @param ty vertical translation
   */
  void setAffine(float a, float b, float c, float d, float tx, float ty);

  /**
   * Copies nine scalar values contained by Matrix into buffer, in member value ascending order:
   * ScaleX, SkewX, TransX, SkewY, ScaleY, TransY, Persp0, Persp1, Persp2.
   * @param buffer  storage for nine scalar values
   */
  void get9(float buffer[9]) const;

  /**
   * Sets Matrix to nine scalar values in buffer, in member value ascending order: ScaleX,
   * SkewX, TransX, SkewY, ScaleY, TransY, Persp0, Persp1, Persp2.
   *
   * Sets matrix to:
   *
   *     | buffer[0] buffer[1] buffer[2] |
   *     | buffer[3] buffer[4] buffer[5] |
   *     | buffer[6] buffer[7] buffer[8] |
   *
   * @param buffer  nine scalar values
   */
  void set9(const float buffer[9]);

  /**
   * Sets Matrix to identity; which has no effect on mapped Point. Sets Matrix to:
   *
   *       | 1 0 0 |
   *       | 0 1 0 |
   *       | 0 0 1 |
   *
   * Also called setIdentity(); use the one that provides better inline documentation.
   */
  void reset();

  /**
   * Sets Matrix to identity; which has no effect on mapped Point. Sets Matrix to:
   *
   *       | 1 0 0 |
   *       | 0 1 0 |
   *       | 0 0 1 |
   *
   *  Also called reset(); use the one that provides better inline documentation.
   */
  void setIdentity() {
    this->reset();
  }

  /**
   * Sets Matrix to translate by (dx, dy).
   * @param dx  horizontal translation
   * @param dy  vertical translation
   */
  void setTranslate(float dx, float dy);

  /**
   * Sets Matrix to scale by sx and sy, about a pivot point at (px, py). The pivot point is
   * unchanged when mapped with Matrix.
   * @param sx  horizontal scale factor
   * @param sy  vertical scale factor
   * @param px  pivot on x-axis
   * @param py  pivot on y-axis
   */
  void setScale(float sx, float sy, float px, float py);

  /**
   * Sets Matrix to scale by sx and sy about at pivot point at (0, 0).
   * @param sx  horizontal scale factor
   * @param sy  vertical scale factor
   */
  void setScale(float sx, float sy);

  /**
   * Sets Matrix to rotate by degrees about a pivot point at (px, py). The pivot point is
   * unchanged when mapped with Matrix. Positive degrees rotates clockwise.
   *  @param degrees  angle of axes relative to upright axes
   *  @param px       pivot on x-axis
   *  @param py       pivot on y-axis
   */
  void setRotate(float degrees, float px, float py);

  /**
   * Sets Matrix to rotate by degrees about a pivot point at (0, 0). Positive degrees rotates
   * clockwise.
   * @param degrees  angle of axes relative to upright axes
   */
  void setRotate(float degrees);

  /**
   * Sets Matrix to rotate by sinValue and cosValue, about a pivot point at (px, py).
   * The pivot point is unchanged when mapped with Matrix.
   * Vector (sinValue, cosValue) describes the angle of rotation relative to (0, 1).
   * Vector length specifies the scale factor.
   */
  void setSinCos(float sinV, float cosV, float px, float py);

  /**
   * Sets Matrix to rotate by sinValue and cosValue, about a pivot point at (0, 0).
   * Vector (sinValue, cosValue) describes the angle of rotation relative to (0, 1).
   * Vector length specifies the scale factor.
   */
  void setSinCos(float sinV, float cosV);

  /**
   * Sets Matrix to skew by kx and ky, about a pivot point at (px, py). The pivot point is
   * unchanged when mapped with Matrix.
   * @param kx  horizontal skew factor
   * @param ky  vertical skew factor
   * @param px  pivot on x-axis
   * @param py  pivot on y-axis
   */
  void setSkew(float kx, float ky, float px, float py);

  /**
   * Sets Matrix to skew by kx and ky, about a pivot point at (0, 0).
   * @param kx  horizontal skew factor
   * @param ky  vertical skew factor
   */
  void setSkew(float kx, float ky);

  /**
   * Sets Matrix to Matrix a multiplied by Matrix b. Either a or b may be this.
   *
   * Given:
   *
   *          | A B C |      | J K L |
   *      a = | D E F |, b = | M N O |
   *          | G H I |      | P Q R |
   *
   * sets Matrix to:
   *
   *              | A B C |   | J K L |   | AJ+BM+CP AK+BN+CQ AL+BO+CR |
   *      a * b = | D E F | * | M N O | = | DJ+EM+FP DK+EN+FQ DL+EO+FR |
   *              | G H I |   | P Q R |   | GJ+HM+IP GK+HN+IQ GL+HO+IR |
   *
   * @param a  Matrix on the left side of multiply expression
   * @param b  Matrix on the right side of multiply expression
   */
  void setConcat(const Matrix& a, const Matrix& b);

  /**
   * Preconcats the matrix with the specified scale. M' = M * S(sx, sy)
   */
  void preTranslate(float dx, float dy);

  /**
   * Postconcats the matrix with the specified scale. M' = S(sx, sy, px, py) * M
   */
  void preScale(float sx, float sy, float px, float py);

  /**
   * Preconcats the matrix with the specified scale. M' = M * S(sx, sy)
   */
  void preScale(float sx, float sy);

  /**
   * Preconcats the matrix with the specified rotation. M' = M * R(degrees, px, py)
   */
  void preRotate(float degrees, float px, float py);

  /**
   * Preconcats the matrix with the specified rotation. M' = M * R(degrees)
   */
  void preRotate(float degrees);

  /**
   * Preconcats the matrix with the specified skew. M' = M * K(kx, ky, px, py)
   */
  void preSkew(float kx, float ky, float px, float py);

  /**
   * Preconcats the matrix with the specified skew. M' = M * K(kx, ky)
   */
  void preSkew(float kx, float ky);

  /**
   * Preconcats the matrix with the specified matrix. M' = M * other
   */
  void preConcat(const Matrix& other);

  /**
   * Postconcats the matrix with the specified translation. M' = T(dx, dy) * M
   */
  void postTranslate(float dx, float dy);

  /**
   * Postconcats the matrix with the specified scale. M' = S(sx, sy, px, py) * M
   */
  void postScale(float sx, float sy, float px, float py);

  /**
   * Postconcats the matrix with the specified scale. M' = S(sx, sy) * M
   */
  void postScale(float sx, float sy);

  /**
   * Postconcats the matrix with the specified rotation. M' = R(degrees, px, py) * M
   */
  void postRotate(float degrees, float px, float py);

  /**
   * Postconcats the matrix with the specified rotation. M' = R(degrees) * M
   */
  void postRotate(float degrees);

  /**
   * Postconcats the matrix with the specified skew. M' = K(kx, ky, px, py) * M
   */
  void postSkew(float kx, float ky, float px, float py);

  /**
   * Postconcats the matrix with the specified skew. M' = K(kx, ky) * M
   */
  void postSkew(float kx, float ky);

  /**
   * Postconcats the matrix with the specified matrix. M' = other * M
   */
  void postConcat(const Matrix& other);

  /**
   * If this matrix can be inverted, return true and if the inverse is not null, set inverse to be
   * the inverse of this matrix. If this matrix cannot be inverted, ignore the inverse and return false.
   */
  bool invert(Matrix* inverse) const;

  /**
   * Returns true if the Matrix is invertible.
   * @return
   */
  bool invertible() const;

  /**
   * Maps src Point array of length count to dst Point array of equal or greater length. Points are
   * mapped by multiplying each Point by Matrix. Given:
   *
   *                | A B C |        | x |
   *       Matrix = | D E F |,  pt = | y |
   *                | G H I |        | 1 |
   *
   * where
   *
   *       for (i = 0; i < count; ++i) {
   *           x = src[i].fX
   *           y = src[i].fY
   *       }
   *
   * each dst Point is computed as:
   *
   *                     |A B C| |x|                               Ax+By+C   Dx+Ey+F
   *       Matrix * pt = |D E F| |y| = |Ax+By+C Dx+Ey+F Gx+Hy+I| = ------- , -------
   *                     |G H I| |1|                               Gx+Hy+I   Gx+Hy+I
   *
   * src and dst may point to the same storage.
   *
   * @param dst    storage for mapped Point
   * @param src    Point to transform
   * @param count  number of Points to transform
   */
  void mapPoints(Point dst[], const Point src[], int count) const;

  /**
   * Maps pts Point array of length count in place. Point are mapped by multiplying each Point by
   * Matrix. Given:
   *
   *                 | A B C |        | x |
   *        Matrix = | D E F |,  pt = | y |
   *                 | G H I |        | 1 |
   *
   * where
   *
   *        for (i = 0; i < count; ++i) {
   *            x = pts[i].fX
   *            y = pts[i].fY
   *        }
   *
   * each resulting pts Point is computed as:
   *
   *                      |A B C| |x|                               Ax+By+C   Dx+Ey+F
   *        Matrix * pt = |D E F| |y| = |Ax+By+C Dx+Ey+F Gx+Hy+I| = ------- , -------
   *                      |G H I| |1|                               Gx+Hy+I   Gx+Hy+I
   *
   * @param pts    storage for mapped Point
   * @param count  number of Points to transform
   */
  void mapPoints(Point pts[], int count) const {
    this->mapPoints(pts, pts, count);
  }

  /**
   * Maps Point (x, y) to result. Point is mapped by multiplying by Matrix. Given:
   *
   *                | A B C |        | x |
   *       Matrix = | D E F |,  pt = | y |
   *                | G H I |        | 1 |
   *
   * the result is computed as:
   *
   *                     |A B C| |x|                               Ax+By+C   Dx+Ey+F
   *       Matrix * pt = |D E F| |y| = |Ax+By+C Dx+Ey+F Gx+Hy+I| = ------- , -------
   *                     |G H I| |1|                               Gx+Hy+I   Gx+Hy+I
   *
   * @param x       x-axis value of Point to map
   * @param y       y-axis value of Point to map
   * @param result  storage for mapped Point
   */
  void mapXY(float x, float y, Point* result) const;

  /**
   * Returns Point (x, y) multiplied by Matrix. Given:
   *
   *                | A B C |        | x |
   *       Matrix = | D E F |,  pt = | y |
   *                | G H I |        | 1 |
   *
   * the result is computed as:
   *
   *                     |A B C| |x|                               Ax+By+C   Dx+Ey+F
   *       Matrix * pt = |D E F| |y| = |Ax+By+C Dx+Ey+F Gx+Hy+I| = ------- , -------
   *                     |G H I| |1|                               Gx+Hy+I   Gx+Hy+I
   *
   * @param x  x-axis value of Point to map
   * @param y  y-axis value of Point to map
   * @return   mapped Point
   */
  Point mapXY(float x, float y) const {
    Point result = {};
    this->mapXY(x, y, &result);
    return result;
  }

  /**
   * Sets dst to bounds of src corners mapped by Matrix.
   */
  void mapRect(Rect* dst, const Rect& src) const;

  /**
   * Sets rect to bounds of rect corners mapped by Matrix. Returns true if mapped corners are
   * computed rect corners.
   */
  void mapRect(Rect* rect) const {
    mapRect(rect, *rect);
  }

  /**
   * Returns bounds of src corners mapped by Matrix.
   */
  Rect mapRect(const Rect& src) const {
    Rect dst = {};
    mapRect(&dst, src);
    return dst;
  }

  /** Compares a and b; returns true if a and b are numerically equal. Returns true even if sign
   * of zero values are different. Returns false if either Matrix contains NaN, even if the other
   * Matrix also contains NaN.
   */
  friend PAG_API bool operator==(const Matrix& a, const Matrix& b);

  /**
   * Compares a and b; returns true if a and b are not numerically equal. Returns false even if
   * sign of zero values are different. Returns true if either Matrix contains NaN, even if the
   * other Matrix also contains NaN.
   */
  friend bool operator!=(const Matrix& a, const Matrix& b) {
    return !(a == b);
  }

  /**
   * Returns the minimum scaling factor of the Matrix by decomposing the scaling and skewing
   * elements. Returns -1 if scale factor overflows.
   */
  float getMinScale() const;

  /**
   * Returns the maximum scaling factor of the Matrix by decomposing the scaling and skewing
   * elements. Returns -1 if scale factor overflows.
   */
  float getMaxScale() const;

  /**
   * Returns true if all elements of the matrix are finite. Returns false if any element is
   * infinity, or NaN.
   */
  bool isFinite() const;

  /**
   * Returns reference to const identity Matrix. Returned Matrix is set to:
   *
   *       | 1 0 0 |
   *       | 0 1 0 |
   *       | 0 0 1 |
   *
   * @return  const identity Matrix
   */
  static const Matrix& I();

  /**
   * Returns reference to a const Matrix with invalid values. Returned Matrix is set to:
   *
   *    | FLT_MAX FLT_MAX FLT_MAX |
   *    | FLT_MAX FLT_MAX FLT_MAX |
   *    |    0       0       1    |
   *
   * @return  const invalid Matrix
   */
  static const Matrix& Invalid();

 private:
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"
  float values[9];
  mutable int32_t typeMask;
  /**
   * Matrix organizes its values in row order. These members correspond to each value in Matrix.
   */
  static constexpr int SCALE_X = 0;  //!< horizontal scale factor
  static constexpr int SKEW_X = 1;   //!< horizontal skew factor
  static constexpr int TRANS_X = 2;  //!< horizontal translation
  static constexpr int SKEW_Y = 3;   //!< vertical skew factor
  static constexpr int SCALE_Y = 4;  //!< vertical scale factor
  static constexpr int TRANS_Y = 5;  //!< vertical translation
#pragma clang diagnostic pop
};

/**
 * Layers are always one of the following types.
 */
enum class RTTR_AUTO_REGISTER_CLASS LayerType {
  Unknown,
  Null,
  Solid,
  Text,
  Shape,
  Image,
  PreCompose,
  Camera,
};

/**
 * Defines the rules on how to scale the content to fit the specified area.
 */
enum class PAG_API PAGScaleMode : uint8_t {
  /**
   * The content is not scaled.
   */
  None = 0,
  /**
   * The content is stretched to fit.
   */
  Stretch = 1,
  /**
   * The content is scaled with respect to the original unscaled image's aspect ratio.
   * This is the default value.
   */
  LetterBox = 2,
  /**
   * The content is scaled to fit with respect to the original unscaled image's aspect ratio.
   * This results in cropping on one axis.
   */
  Zoom = 3
};

/**
 * Defines the rules on how to stretch the timeline of content to fit the specified duration.
 */
enum class PAG_API PAGTimeStretchMode : uint8_t {
  /**
   * Keep the original playing speed, and display the last frame if the content's duration is less
   * than target duration.
   */
    None = 0,
  /*
   * Change the playing speed of the content to fit target duration.
   */
    Scale = 1,
  /**
   * Keep the original playing speed, but repeat the content if the content's duration is less than
   * target duration. This is the default mode.
   */
    Repeat = 2,
  /**
   * Keep the original playing speed, but repeat the content in reversed if the content's duration
   * is less than target duration.
   */
    RepeatInverted = 3
};

enum class PAG_API ParagraphJustification : uint8_t {
  LeftJustify = 0,
  CenterJustify = 1,
  RightJustify = 2,
  FullJustifyLastLineLeft = 3,
  FullJustifyLastLineRight = 4,
  FullJustifyLastLineCenter = 5,
  FullJustifyLastLineFull = 6
};

enum class PAG_API TextDirection : uint8_t {
  Default = 0,
  Horizontal = 1,
  Vertical = 2
};

/**
 * The Text object stores a value for a TextLayer's Source Text property.
 */
class PAG_API TextDocument {
 public:
  /**
   * When true, the text layer shows a fill.
   */
  bool applyFill = true;

  /**
   * When true, the text layer shows a stroke.
   */
  bool applyStroke = false;

  /**
   * Readonly, external modifications are not valid.
   */
  float baselineShift = 0;

  /**
   * When true, the text layer is paragraph (bounded) text.
   * Readonly, external modifications are not valid.
   */
  bool boxText = false;

  /**
   * Readonly, external modifications are not valid.
   */
  Point boxTextPos = Point::Zero();

  /**
   * For box text, the pixel dimensions for the text bounds.
   * Readonly, external modifications are not valid.
   */
  Point boxTextSize = Point::Zero();

  /**
   * Readonly, external modifications are not valid.
   */
  float firstBaseLine = 0;

  bool fauxBold = false;

  bool fauxItalic = false;
  /**
   * The text layer’s fill color.
   */
  Color fillColor = Black;

  /**
   * A string with the name of the font family.
   **/
  std::string fontFamily = "";

  /**
   * A string with the style information — e.g., “bold”, “italic”.
   **/
  std::string fontStyle = "";

  /**
   * The text layer’s font size in pixels.
   */
  float fontSize = 24;

  /**
   * The text layer’s stroke color.
   */
  Color strokeColor = Black;

  /**
   * Indicates the rendering order for the fill and stroke of a text layer.
   * Readonly, external modifications are not valid.
   */
  bool strokeOverFill = true;

  /**
   * The text layer’s stroke thickness.
   */
  float strokeWidth = 1;

  /**
   * The text layer’s Source Text value.
   */
  std::string text = "";

  /**
   * The paragraph justification for the text layer.
   */
  ParagraphJustification justification = ParagraphJustification::LeftJustify;

  /**
   * The space between lines, 0 indicates 'auto', which is fontSize * 1.2
   */
  float leading = 0;

  /**
   * The text layer’s spacing between characters.
   */
  float tracking = 0;

  /**
   *  The text layer’s background color
   */
  Color backgroundColor = White;
  /**
   *  The text layer’s background alpha. 0 = 100% transparent, 255 = 100% opaque.
   */
  uint8_t backgroundAlpha = 0;

  TextDirection direction = TextDirection::Default;
};

typedef std::shared_ptr<TextDocument> TextDocumentHandle;

/**
 * Marker stores comments and other metadata and mark important times in a composition or layer.
 */
struct RTTR_AUTO_REGISTER_CLASS Marker {
  Frame startTime = 0;
  Frame duration = 0;
  std::string comment;
};

/**
 * A container for data of bytes.
 */
class PAG_API ByteData {
 public:
  /**
   * Creates a ByteData object from the specified file path.
   */
  static std::unique_ptr<ByteData> FromPath(const std::string& filePath);
  /**
   * Creates a ByteData object and copy the specified data into it.
   */
  static std::unique_ptr<ByteData> MakeCopy(const void* data, size_t length);
  /**
   *  Call this when the data parameter is already const and will outlive the lifetime of the
   *  ByteData. Suitable for with const globals.
   */
  static std::unique_ptr<ByteData> MakeWithoutCopy(void* data, size_t length);
  /**
   * Creates a ByteData object and take ownership of the specified data. The specified data will be
   * deleted when the returned ByteData is released.
   * Set releaseCallback to release outer data, and default protocol is to call 'delete[] data',
   * if data is created by 'malloc', you need send a function to call 'free(data)'.
   */
  static std::unique_ptr<ByteData> MakeAdopted(
      uint8_t* data, size_t length, std::function<void(uint8_t*)> releaseCallback = DeleteCallback);
  /**
   * Creates a ByteData object with specified length.
   */
  static std::unique_ptr<ByteData> Make(size_t length);

  ~ByteData() {
    if (_releaseCallback) {
      _releaseCallback(_data);
    }
  }

  /**
   * Returns the memory address of byte data.
   */
  uint8_t* data() const {
    return _data;
  }

  /**
   * Returns the byte size.
   */
  size_t length() const {
    return _length;
  }

 private:
  static void DeleteCallback(uint8_t* data) {
    if (data) delete[] data;
  }

  ByteData(uint8_t* data, size_t length,
           std::function<void(uint8_t*)> releaseCallback = DeleteCallback)
      : _data(data), _length(length), _releaseCallback(std::move(releaseCallback)) {
  }

  uint8_t* _data;
  size_t _length;
  const std::function<void(uint8_t*)> _releaseCallback;
};

/**
 * Describes how to interpret the alpha component of a pixel.
 */
enum class AlphaType {
  /**
   * uninitialized.
   */
  Unknown,
  /**
   * pixel is opaque.
   */
  Opaque,
  /**
   * pixel components are premultiplied by alpha.
   */
  Premultiplied,
  /**
   * pixel components are independent of alpha.
   */
  Unpremultiplied,
};

/**
 * Describes how pixel bits encode color. These values match up with the enum in Bitmap.Config on
 * Android platform.
 */
enum class ColorType {
  /**
   * uninitialized.
   */
  Unknown,
  /**
   * Each pixel is stored as a single translucency (alpha) channel. This is very useful for
   * storing masks efficiently, for instance. No color information is stored. With this
   * configuration, each pixel requires 1 byte of memory.
   */
  ALPHA_8,
  /**
   * Each pixel is stored on 4 bytes. Each channel (RGB and alpha for translucency) is stored with 8
   * bits of precision (256 possible values). The channel order is: red, green, blue, alpha.
   */
  RGBA_8888,
  /**
   * Each pixel is stored on 4 bytes. Each channel (RGB and alpha for translucency) is stored with 8
   * bits of precision (256 possible values). The channel order is: blue, green, red, alpha.
   */
  BGRA_8888,
  /**
   * Each pixel is stored on 2 bytes, and only the RGB channels are encoded: red is stored with 5
   * bits of precision (32 possible values), green is stored with 6 bits of precision (64 possible
   * values), and blue is stored with 5 bits of precision.
   */
  RGB_565,
  /**
   * Each pixel is stored as a single grayscale level. No color information is stored. With this
   * configuration, each pixel requires 1 byte of memory.
   */
  Gray_8,
  /**
   * Each pixel is stored on 8 bytes. Each channel (RGB and alpha for translucency) is stored as a
   * half-precision floating point value. This configuration is particularly suited for wide-gamut
   * and HDR content.
   */
  RGBA_F16,
  /**
   * Each pixel is stored on 4 bytes. Each RGB channel is stored with 10 bits of precision (1024
   * possible values). There is an additional alpha channel that is stored with 2 bits of precision
   * (4 possible values). This configuration is suited for wide-gamut and HDR content which does not
   * require alpha blending, such that the memory cost is the same as RGBA_8888 while enabling
   * higher color precision.
   */
  RGBA_1010102
};

enum class PAG_API BlendMode : uint8_t {
  Normal = 0,
  Multiply = 1,
  Screen = 2,
  Overlay = 3,
  Darken = 4,
  Lighten = 5,
  ColorDodge = 6,
  ColorBurn = 7,
  HardLight = 8,
  SoftLight = 9,
  Difference = 10,
  Exclusion = 11,
  Hue = 12,
  Saturation = 13,
  Color = 14,
  Luminosity = 15,
  Add = 16
};

}  // namespace pag
