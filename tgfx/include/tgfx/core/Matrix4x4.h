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
 * Matrix4x4 holds a 3x3 matrix for transforming coordinates. This allows mapping Point and vectors
 * with translation, scaling, skewing, rotation, and perspective.
 * Matrix4x4 elements are in row major order. Matrix4x4 does not have a constructor, so it must be
 * explicitly initialized. setIdentity() initializes Matrix4x4 so it has no effect.
 * setTranslate(), setScale(), setSkew(), setRotate(), set9 and setAll() initializes all Matrix4x4
 * elements with the corresponding mapping.
 */
class Matrix4x4 {
 public:
  /**
   * Sets Matrix4x4 to scale by (sx, sy, sz). Returned matrix is:
   *
   *       | sx  0  0  0|
   *       |  0 sy  0  0|
   *       |  0  0 sz  0|
   *       |  0  0  0  1|
   *
   *  @param sx  scale factor on x-axis
   *  @param sy  scale factor on y-axis
   *  @param sz  scale factor on z-axis
   *  @return    Matrix4x4 with scale
   */
  static Matrix4x4 MakeScale(float sx, float sy, float sz) {
    Matrix4x4 m = {};
    m.setScale(sx, sy, sz);
    return m;
  }

  /**
   * Sets Matrix4x4 to scale by (scale, scale, scale). Returned matrix is:
   *
   *      | scale   0     0     0 |
   *      |   0   scale   0     0 |
   *      |   0     0   scale   0 |
   *      |   0     0     0     1 |
   *
   * @param scale  x y scale factor on z-axis
   * @return       Matrix4x4 with scale
   */
  static Matrix4x4 MakeScale(float scale) {
    Matrix4x4 m = {};
    m.setScale(scale, scale, scale);
    return m;
  }

  /**
   * Sets Matrix4x4 to translate by (dx, dy, dz). Returned matrix is:
   *
   *       | 1 0 0 dx |
   *       | 0 1 0 dy |
   *       | 0 0 1 dz |
   *       | 0 0 0 1  |
   *
   * @param dx  translation on x-axis
   * @param dy  translation on y-axis
   * @param dz  translation on z-axis
   * @return    Matrix4x4 with translation
   */
  static Matrix4x4 MakeTrans(float dx, float dy, float dz) {
    Matrix4x4 m = {};
    m.setTranslate(dx, dy, dz);
    return m;
  }

  /**
   * Sets Matrix4x4 to:
   *
   *      |  m1   m2   m3   m4  |
   *      |  m5   m6   m7   m8  |
   *      |  m9   m10  m11  m12 |
   *      |  m13  m14  m15  m16 |
   *
   * @return        Matrix4x4 constructed from parameters
   */
  static Matrix4x4 MakeAll(float m1,  float m2,  float m3,  float m4,
                           float m5,  float m6,  float m7,  float m8,
                           float m9,  float m10, float m11, float m12,
                           float m13, float m14, float m15, float m16) {
    Matrix4x4 m = {};
    m.setAll(m1,  m2,  m3,  m4,
             m5,  m6,  m7,  m8,
             m9,  m10, m11, m12,
             m13, m14, m15, m16);
    return m;
  }

  /**
   * Returns true if Matrix4x4 is identity.  Identity matrix is:
   *
   *       | 1 0 0 |
   *       | 0 1 0 |
   *       | 0 0 1 |
   *
   * @return  true if Matrix4x4 has no effect
   */
  bool isIdentity() const {
    return values[0] == 1 && values[1] == 0 && values[2] == 0 && values[3] == 0 && values[4] == 1 &&
           values[5] == 0 && values[6] == 0 && values[7] == 0 && values[8] == 1;
  }

  /**
   * Returns one matrix value.
   */
  float operator[](int index) const {
    return values[index];
  }

  /**
   * Returns one matrix value.
   */
  float get(int index) const {
    return values[index];
  }

  /**
   * Returns writable Matrix4x4 value.
   */
  float& operator[](int index) {
    return values[index];
  }

  /**
   * Sets Matrix4x4 value.
   */
  void set(int index, float value) {
    values[index] = value;
  }

  /**
   * Sets horizontal scale factor.
   * @param v  horizontal scale factor to store
   */
  void setScaleX(float v) {
    this->set(SCALE_X, v);
  }

  /**
   * Sets vertical scale factor.
   * @param v  vertical scale factor to store
   */
  void setScaleY(float v) {
    this->set(SCALE_Y, v);
  }

  /**
   * Sets vertical skew factor.
   * @param v  vertical skew factor to store
   */
  void setSkewY(float v) {
    this->set(SKEW_Y, v);
  }

  /**
   * Sets horizontal skew factor.
   * @param v  horizontal skew factor to store
   */
  void setSkewX(float v) {
    this->set(SKEW_X, v);
  }

  /**
   * Sets horizontal translation.
   * @param v  horizontal translation to store
   */
  void setTranslateX(float v) {
    this->set(TRANS_X, v);
  }

  /**
   * Sets vertical translation.
   * @param v  vertical translation to store
   */
  void setTranslateY(float v) {
    this->set(TRANS_Y, v);
  }

  /**
   * Sets all values from parameters. Sets matrix to:
   *
   *      |  m1   m2   m3   m4  |
   *      |  m5   m6   m7   m8  |
   *      |  m9   m10  m11  m12 |
   *      |  m13  m14  m15  m16 |
   *
   */
  void setAll(float m1,  float m2,  float m3,  float m4,
              float m5,  float m6,  float m7,  float m8,
              float m9,  float m10, float m11, float m12,
              float m13, float m14, float m15, float m16);

  void setAffine(float a, float b, float c, float d, float tx, float ty);

  /**
   * Copies nine scalar values contained by Matrix4x4 into buffer, in member value ascending order:
   * ScaleX, SkewX, TransX, SkewY, ScaleY, TransY, Persp0, Persp1, Persp2.
   * @param buffer  storage for nine scalar values
   */
  void get16(float buffer[16]) const {
    memcpy(buffer, values, 16 * sizeof(float));
  }

  /**
   * Sets Matrix4x4 to nine scalar values in buffer, in member value ascending order: ScaleX,
   * SkewX, TransX, SkewY, ScaleY, TransY, Persp0, Persp1, Persp2.
   *
   * Sets matrix to:
   *
   *     | buffer[0]  buffer[1]  buffer[2]  buffer[3]  |
   *     | buffer[4]  buffer[5]  buffer[6]  buffer[7]  |
   *     | buffer[8]  buffer[9]  buffer[10] buffer[11] |
   *     | buffer[12] buffer[13] buffer[14] buffer[15] |
   *
   * @param buffer  nine scalar values
   */
  void set16(const float buffer[16]) {
    memcpy(values, buffer, 16 * sizeof(float));
  }

  /**
   * Sets Matrix4x4 to identity; which has no effect on mapped Point. Sets Matrix4x4 to:
   *
   *       | 1 0 0 0 |
   *       | 0 1 0 0 |
   *       | 0 0 1 0 |
   *       | 0 0 0 1 |
   *
   * Also called setIdentity(); use the one that provides better inline documentation.
   */
  void reset();

  /**
   * Sets Matrix4x4 to identity; which has no effect on mapped Point. Sets Matrix4x4 to:
   *
   *       | 1 0 0 0 |
   *       | 0 1 0 0 |
   *       | 0 0 1 0 |
   *       | 0 0 0 1 |
   *
   *  Also called reset(); use the one that provides better inline documentation.
   */
  void setIdentity() {
    this->reset();
  }

  /**
   * Sets Matrix4x4 to translate by (dx, dy, dz).
   * @param dx  translation on x-axis
   * @param dy  translation on y-axis
   * @param dz  translation on z-axis
   */
  void setTranslate(float dx, float dy, float dz);

  /**
   * Sets Matrix4x4 to scale by sx sy and sz, about a pivot point at (px, py, pz). The pivot
   * point is unchanged when mapped with Matrix4x4.
   * @param sx  scale factor on x-axis
   * @param sy  scale factor on y-axis
   * @param sz  scale factor on z-axis
   * @param px  pivot on x-axis
   * @param py  pivot on y-axis
   * @param pz  pivot on z-axis
   */
  void setScale(float sx, float sy, float sz, float px, float py, float pz);

  /**
   * Sets Matrix4x4 to scale by sx and sy about at pivot point at (0, 0, 0).
   * @param sx  scale factor on x-axis
   * @param sy  scale factor on y-axis
   * @param sz  scale factor on z-axis
   */
  void setScale(float sx, float sy, float sz);

  /**
   * Sets Matrix4x4 to rotate by degrees about a pivot point at (px, py). The pivot point is
   * unchanged when mapped with Matrix4x4. Positive degrees rotates clockwise.
   *  @param degrees  angle of axes relative to upright axes
   *  @param px       pivot on x-axis
   *  @param py       pivot on y-axis
   *  @param pz       pivot on z-axis
   */
  void setRotate(float degrees, float px, float py, float pz);

  /**
   * Sets Matrix4x4 to rotate by degrees about a pivot point at (0, 0). Positive degrees rotates
   * clockwise.
   * @param degrees  angle of axes relative to upright axes
   */
  void setRotate(float degrees);

  /**
   * Sets Matrix4x4 to Matrix4x4 a multiplied by Matrix4x4 b.
   * @param a  Matrix4x4 on left side of multiply expression
   * @param b  Matrix4x4 on right side of multiply expression
   */
  void setConcat(const Matrix4x4& a, const Matrix4x4& b);

  /**
   * Preconcats the matrix with the specified translation. M' = M * T(dx, dy, dz)
   */
  void preTranslate(float dx, float dy, float dz);

  /**
   * Postconcats the matrix with the specified scale. M' = S(sx, sy, sz, px, py, pz) * M
   */
  void preScale(float sx, float sy, float sz, float px, float py, float pz);

  /**
   * Preconcats the matrix with the specified scale. M' = M * S(sx, sy, sz)
   */
  void preScale(float sx, float sy, float sz);

  /**
   * Preconcats the matrix with the specified rotation. M' = M * R(degrees, px, py, pz)
   */
  void preRotate(float degrees, float px, float py, float pz);

  /**
   * Preconcats the matrix with the specified rotation. M' = M * R(degrees)
   */
  void preRotate(float degrees);

  /**
   * Preconcats the matrix with the specified skew. M' = M * K(kx, ky, kz, px, py, pz)
   */
  void preSkew(float kx, float ky, float kz, float px, float py, float pz);

  /**
   * Preconcats the matrix with the specified skew. M' = M * K(kx, ky, kz)
   */
  void preSkew(float kx, float ky, float kz);

  /**
   * Preconcats the matrix with the specified matrix. M' = M * other
   */
  void preConcat(const Matrix4x4& other);

  /**
   * Postconcats the matrix with the specified translation. M' = T(dx, dy, dz) * M
   */
  void postTranslate(float dx, float dy, float dz);

  /**
   * Postconcats the matrix with the specified scale. M' = S(sx, sy, sz, px, py, pz) * M
   */
  void postScale(float sx, float sy, float sz, float px, float py, float pz);

  /**
   * Postconcats the matrix with the specified scale. M' = S(sx, sy, sz) * M
   */
  void postScale(float sx, float sy, float sz);

  /**
   * Postconcats the matrix with the specified rotation. M' = R(degrees, px, py, pz) * M
   */
  void postRotate(float degrees, float px, float py, float pz);

  /**
   * Postconcats the matrix with the specified rotation. M' = R(degrees) * M
   */
  void postRotate(float degrees);

  /**
   * Postconcats the matrix with the specified skew. M' = K(kx, ky, kz, px, py, pz) * M
   */
  void postSkew(float kx, float ky, float kz, float px, float py, float pz);

  /**
   * Postconcats the matrix with the specified skew. M' = K(kx, ky, kz) * M
   */
  void postSkew(float kx, float ky, float kz);

  /**
   * Postconcats the matrix with the specified matrix. M' = other * M
   */
  void postConcat(const Matrix4x4& other);

  /**
   * If this matrix can be inverted, return true and if inverse is not null, set inverse to be the
   * inverse of this matrix. If this matrix cannot be inverted, ignore inverse and return false.
   */
  bool invert(Matrix4x4* inverse) const {
    if (this->isIdentity()) {
      if (inverse) {
        inverse->reset();
      }
      return true;
    }
    return this->invertNonIdentity(inverse);
  }

  bool invertible() const;

  /** Compares a and b; returns true if a and b are numerically equal. Returns true even if sign
   * of zero values are different. Returns false if either Matrix4x4 contains NaN, even if the other
   * Matrix4x4 also contains NaN.
   */
  friend bool operator==(const Matrix4x4& a, const Matrix4x4& b);

  /**
   * Compares a and b; returns true if a and b are not numerically equal. Returns false even if
   * sign of zero values are different. Returns true if either Matrix4x4 contains NaN, even if the
   * other Matrix4x4 also contains NaN.
   */
  friend bool operator!=(const Matrix4x4& a, const Matrix4x4& b) {
    return !(a == b);
  }

  /**
   * Returns the minimum scaling factor of Matrix4x4 by decomposing the scaling and skewing elements.
   * Returns -1 if scale factor overflows or Matrix4x4 contains perspective.
   */
  float getMinScale() const;

  /**
   * Returns the maximum scaling factor of Matrix4x4 by decomposing the scaling and skewing elements.
   * Returns -1 if scale factor overflows or Matrix4x4 contains perspective.
   */
  float getMaxScale() const;

  /**
   * Returns true if all elements of the matrix are finite. Returns false if any
   * element is infinity, or NaN.
   */
  bool isFinite() const;

  /**
   * Returns reference to const identity Matrix4x4. Returned Matrix4x4 is set to:
   *
   *       | 1 0 0 0 |
   *       | 0 1 0 0 |
   *       | 0 0 1 0 |
   *       | 0 0 0 1 |
   *
   *   @return  const identity Matrix4x4
   */
  static const Matrix4x4& I();

 private:
  float values[16];
  /**
   * Matrix4x4 organizes its values in row order.
   */

  void setScaleTranslate(float sx, float sy, float sz, float tx, float ty, float tz);
  bool invertNonIdentity(Matrix4x4* inverse) const;
  bool getMinMaxScaleFactors(float results[2]) const;
};
}  // namespace tgfx
