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

#include <cstring>
#include "tgfx/core/Rect.h"

namespace tgfx {
/***
 * Matrix holds a 3x2 matrix for transforming coordinates. This allows mapping Point and vectors
 * with translation, scaling, skewing, and rotation. Together these types of transformations are
 * known as affine transformations. Affine transformations preserve the straightness of lines while
 * transforming, so that parallel lines stay parallel. Matrix elements are in row major order.
 * Matrix does not have a constructor, so it must be explicitly initialized.
 */
class Matrix {
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
   *  @return    Matrix with scale factors.
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
   * @return       Matrix with scale factors.
   */
  static Matrix MakeScale(float scale) {
    Matrix m = {};
    m.setScale(scale, scale);
    return m;
  }

  /**
   * Sets Matrix to translate by (tx, ty). Returned matrix is:
   *
   *       | 1 0 tx |
   *       | 0 1 ty |
   *       | 0 0  1 |
   *
   * @param tx  horizontal translation
   * @param ty  vertical translation
   * @return    Matrix with translation
   */
  static Matrix MakeTrans(float tx, float ty) {
    Matrix m = {};
    m.setTranslate(tx, ty);
    return m;
  }

  /**
   * Sets Matrix to:
   *
   *      | scaleX  skewX transX |
   *      | skewY  scaleY transY |
   *      |   0      0      1    |
   *
   * @param scaleX  horizontal scale factor
   * @param skewX   horizontal skew factor
   * @param transX  horizontal translation
   * @param skewY   vertical skew factor
   * @param scaleY  vertical scale factor
   * @param transY  vertical translation
   * @return        Matrix constructed from parameters
   */
  static Matrix MakeAll(float scaleX, float skewX, float transX, float skewY, float scaleY,
                        float transY) {
    Matrix m = {};
    m.setAll(scaleX, skewX, transX, skewY, scaleY, transY);
    return m;
  }

  /**
   * Returns true if Matrix is identity. The identity matrix is:
   *
   *       | 1 0 0 |
   *       | 0 1 0 |
   *       | 0 0 1 |
   *
   * @return  Returns true if the Matrix has no effect.
   */
  bool isIdentity() const {
    return values[0] == 1 && values[1] == 0 && values[2] == 0 && values[3] == 0 && values[4] == 1 &&
           values[5] == 0;
  }

  /**
   * Returns one matrix value.
   */
  float operator[](int index) const {
    return values[index];
  }

  /**
   * Returns writable Matrix value.
   */
  float& operator[](int index) {
    return values[index];
  }

  /**
   * Returns one matrix value.
   */
  float get(int index) const {
    return values[index];
  }

  /**
   * Sets Matrix value.
   */
  void set(int index, float value) {
    values[index] = value;
  }

  /**
   * Copies six scalar values contained by Matrix into buffer, in member value ascending order:
   * ScaleX, SkewX, TransX, SkewY, ScaleY, TransY.
   * @param buffer  storage for six scalar values.
   */
  void get6(float buffer[6]) const {
    memcpy(buffer, values, 6 * sizeof(float));
  }

  /**
   * Sets Matrix to six scalar values in buffer, in member value ascending order:
   * ScaleX, SkewX, TransX, SkewY, ScaleY, TransY.
   * Sets matrix to:
   *
   *     | buffer[0] buffer[1] buffer[2] |
   *     | buffer[3] buffer[4] buffer[5] |
   *
   * @param buffer storage for six scalar values.
   */
  void set6(const float buffer[6]) {
    memcpy(values, buffer, 6 * sizeof(float));
  }

  /**
   * Copies nine scalar values contained by Matrix into buffer, in member value ascending order:
   * ScaleX, SkewX, TransX, SkewY, ScaleY, TransY, 0, 0, 1.
   * @param buffer  storage for nine scalar values
   */
  void get9(float buffer[9]) const;

  /**
   * Returns the horizontal scale factor.
   */
  float getScaleX() const {
    return values[SCALE_X];
  }

  /**
   * Returns the vertical scale factor.
   */
  float getScaleY() const {
    return values[SCALE_Y];
  }

  /**
   * Returns the vertical skew factor.
   */
  float getSkewY() const {
    return values[SKEW_Y];
  }

  /**
   * Returns the horizontal scale factor.
   */
  float getSkewX() const {
    return values[SKEW_X];
  }

  /**
   * Returns the horizontal translation factor.
   */
  float getTranslateX() const {
    return values[TRANS_X];
  }

  /**
   * Returns the vertical translation factor.
   */
  float getTranslateY() const {
    return values[TRANS_Y];
  }

  /**
   * Sets the horizontal scale factor.
   */
  void setScaleX(float v) {
    values[SCALE_X] = v;
  }

  /**
   * Sets the vertical scale factor.
   */
  void setScaleY(float v) {
    values[SCALE_Y] = v;
  }

  /**
   * Sets the vertical skew factor.
   */
  void setSkewY(float v) {
    values[SKEW_Y] = v;
  }

  /**
   * Sets the horizontal skew factor.
   */
  void setSkewX(float v) {
    values[SKEW_X] = v;
  }

  /**
   * Sets the horizontal translation.
   */
  void setTranslateX(float v) {
    values[TRANS_X] = v;
  }

  /**
   * Sets the vertical translation.
   */
  void setTranslateY(float v) {
    values[TRANS_Y] = v;
  }

  /**
   * Sets all values from parameters. Sets matrix to:
   *
   *      | scaleX  skewX transX |
   *      | skewY  scaleY transY |
   *      |   0      0      1    |
   *
   * @param scaleX  horizontal scale factor to store
   * @param skewX   horizontal skew factor to store
   * @param transX  horizontal translation to store
   * @param skewY   vertical skew factor to store
   * @param scaleY  vertical scale factor to store
   * @param transY  vertical translation to store
   */
  void setAll(float scaleX, float skewX, float transX, float skewY, float scaleY, float transY);

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
   * Sets Matrix to translate by (tx, ty).
   * @param tx  horizontal translation
   * @param ty  vertical translation
   */
  void setTranslate(float tx, float ty);

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
  void preTranslate(float tx, float ty);

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
   * Postconcats the matrix with the specified translation. M' = T(tx, ty) * M
   */
  void postTranslate(float tx, float ty);

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
   * the inverse of this matrix. If this matrix cannot be inverted, ignore the inverse and return
   * false.
   */
  bool invert(Matrix* inverse) const {
    if (this->isIdentity()) {
      if (inverse) {
        inverse->reset();
      }
      return true;
    }
    return this->invertNonIdentity(inverse);
  }

  /**
   * Returns ture if the Matrix is invertible.
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
   * Maps pts Point array of length count in place. Points are mapped by multiplying each Point by
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
   * Returns true if Matrix maps Rect to another Rect. If true, the Matrix is identity, or scales,
   * or rotates a multiple of 90 degrees, or mirrors on axes. In all cases, Matrix may also have
   * translation. Matrix form is either:
   *
   *         | scale-x    0    translate-x |
   *         |    0    scale-y translate-y |
   *         |    0       0         1      |
   *
   *     or
   *
   *        |    0     rotate-x translate-x |
   *        | rotate-y    0     translate-y |
   *        |    0        0          1      |
   *
   *    for non-zero values of scale-x, scale-y, rotate-x, and rotate-y.
   */
  bool rectStaysRect() const;

  /**
   * Sets dst to bounds of src corners mapped by Matrix.
   */
  void mapRect(Rect* dst, const Rect& src) const;

  /**
   * Sets rect to bounds of rect corners mapped by Matrix.
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
  friend bool operator==(const Matrix& a, const Matrix& b);

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
   *   @return  const identity Matrix
   */
  static const Matrix& I();

 private:
  float values[6];
  /**
   * Matrix organizes its values in row order. These members correspond to each value in Matrix.
   */
  static constexpr int SCALE_X = 0;  //!< horizontal scale factor
  static constexpr int SKEW_X = 1;   //!< horizontal skew factor
  static constexpr int TRANS_X = 2;  //!< horizontal translation
  static constexpr int SKEW_Y = 3;   //!< vertical skew factor
  static constexpr int SCALE_Y = 4;  //!< vertical scale factor
  static constexpr int TRANS_Y = 5;  //!< vertical translation

  void setScaleTranslate(float sx, float sy, float tx, float ty);
  bool invertNonIdentity(Matrix* inverse) const;
  bool getMinMaxScaleFactors(float results[2]) const;
};
}  // namespace tgfx
