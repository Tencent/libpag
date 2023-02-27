/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "tgfx/core/Matrix.h"
#include "tgfx/core/Rect.h"

namespace pag {

struct Vec3 {
  float x, y, z;

  bool operator==(const Vec3& v) const {
    return x == v.x && y == v.y && z == v.z;
  }
  bool operator!=(const Vec3& v) const {
    return !(*this == v);
  }

  static float Dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
  }
  static Vec3 Cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
  }
  static Vec3 Normalize(const Vec3& v) {
    return v * (1.0f / v.length());
  }

  Vec3 operator-() const {
    return {-x, -y, -z};
  }
  Vec3 operator+(const Vec3& v) const {
    return {x + v.x, y + v.y, z + v.z};
  }
  Vec3 operator-(const Vec3& v) const {
    return {x - v.x, y - v.y, z - v.z};
  }

  Vec3 operator*(const Vec3& v) const {
    return {x * v.x, y * v.y, z * v.z};
  }
  friend Vec3 operator*(const Vec3& v, float s) {
    return {v.x * s, v.y * s, v.z * s};
  }
  friend Vec3 operator*(float s, const Vec3& v) {
    return v * s;
  }

  void operator+=(Vec3 v) {
    *this = *this + v;
  }
  void operator-=(Vec3 v) {
    *this = *this - v;
  }
  void operator*=(Vec3 v) {
    *this = *this * v;
  }
  void operator*=(float s) {
    *this = *this * s;
  }

  float lengthSquared() const {
    return Dot(*this, *this);
  }
  float length() const {
    return sqrtf(Dot(*this, *this));
  }

  float dot(const Vec3& v) const {
    return Dot(*this, v);
  }
  Vec3 cross(const Vec3& v) const {
    return Cross(*this, v);
  }
  Vec3 normalize() const {
    return Normalize(*this);
  }

  const float* ptr() const {
    return &x;
  }
  float* ptr() {
    return &x;
  }
};

struct Vec4 {
  float x, y, z, w;

  bool operator==(const Vec4& v) const {
    return x == v.x && y == v.y && z == v.z && w == v.w;
  }
  bool operator!=(const Vec4& v) const {
    return !(*this == v);
  }

  static float Dot(const Vec4& a, const Vec4& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
  }
  static Vec4 Normalize(const Vec4& v) {
    return v * (1.0f / v.length());
  }

  Vec4 operator-() const {
    return {-x, -y, -z, -w};
  }
  Vec4 operator+(const Vec4& v) const {
    return {x + v.x, y + v.y, z + v.z, w + v.w};
  }
  Vec4 operator-(const Vec4& v) const {
    return {x - v.x, y - v.y, z - v.z, w - v.w};
  }

  Vec4 operator*(const Vec4& v) const {
    return {x * v.x, y * v.y, z * v.z, w * v.w};
  }
  friend Vec4 operator*(const Vec4& v, float s) {
    return {v.x * s, v.y * s, v.z * s, v.w * s};
  }
  friend Vec4 operator*(float s, const Vec4& v) {
    return v * s;
  }

  float lengthSquared() const {
    return Dot(*this, *this);
  }
  float length() const {
    return sqrtf(Dot(*this, *this));
  }

  float dot(const Vec4& v) const {
    return Dot(*this, v);
  }
  Vec4 normalize() const {
    return Normalize(*this);
  }

  const float* ptr() const {
    return &x;
  }
  float* ptr() {
    return &x;
  }

  float operator[](int i) const {
    return this->ptr()[i];
  }
  float& operator[](int i) {
    return this->ptr()[i];
  }
};

/**
 *  Matrix3D assumes a right-handed coordinate system:
 *      +X goes to the right
 *      +Y goes down
 *      +Z goes into the screen (away from the viewer)
 */
class Matrix3D {
 public:
  Matrix3D(const Matrix3D& src) = default;
  Matrix3D& operator=(const Matrix3D& src) = default;

  constexpr Matrix3D() : fMat{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1} {
  }

  Matrix3D(const Matrix3D& a, const Matrix3D& b) {
    this->setConcat(a, b);
  }

  enum Uninitialized_Constructor { kUninitialized_Constructor };
  Matrix3D(Uninitialized_Constructor) {
  }

  /**
     *  The constructor parameters are in row-major order.
   */
  constexpr Matrix3D(float m0, float m4, float m8, float m12, float m1, float m5, float m9,
                     float m13, float m2, float m6, float m10, float m14, float m3, float m7,
                     float m11, float m15)
      // fMat is column-major order in memory.
      : fMat{m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15} {
  }

  static Matrix3D Rows(const Vec4& r0, const Vec4& r1, const Vec4& r2, const Vec4& r3) {
    Matrix3D m(kUninitialized_Constructor);
    m.setRow(0, r0);
    m.setRow(1, r1);
    m.setRow(2, r2);
    m.setRow(3, r3);
    return m;
  }
  static Matrix3D Cols(const Vec4& c0, const Vec4& c1, const Vec4& c2, const Vec4& c3) {
    Matrix3D m(kUninitialized_Constructor);
    m.setCol(0, c0);
    m.setCol(1, c1);
    m.setCol(2, c2);
    m.setCol(3, c3);
    return m;
  }

  static Matrix3D RowMajor(const float r[16]) {
    return Matrix3D(r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8], r[9], r[10], r[11], r[12],
                    r[13], r[14], r[15]);
  }
  static Matrix3D ColMajor(const float c[16]) {
    return Matrix3D(c[0], c[4], c[8], c[12], c[1], c[5], c[9], c[13], c[2], c[6], c[10], c[14],
                    c[3], c[7], c[11], c[15]);
  }

  static Matrix3D Translate(float x, float y, float z = 0) {
    return Matrix3D(1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, z, 0, 0, 0, 1);
  }

  static Matrix3D Scale(float x, float y, float z = 1) {
    return Matrix3D(x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, 0, 0, 1);
  }

  static Matrix3D Rotate(Vec3 axis, float radians) {
    Matrix3D m(kUninitialized_Constructor);
    m.setRotate(axis, radians);
    return m;
  }

  // Scales and translates 'src' to fill 'dst' exactly.
  static Matrix3D RectToRect(const tgfx::Rect& src, const tgfx::Rect& dst);

  static Matrix3D LookAt(const Vec3& eye, const Vec3& center, const Vec3& up);
  static Matrix3D Perspective(float near, float far, float angle);

  bool operator==(const Matrix3D& other) const;
  bool operator!=(const Matrix3D& other) const {
    return !(other == *this);
  }

  void getColMajor(float v[]) const {
    memcpy(v, fMat, sizeof(fMat));
  }
  void getRowMajor(float v[]) const;

  float rc(int r, int c) const {
    return fMat[c * 4 + r];
  }
  void setRC(int r, int c, float value) {
    fMat[c * 4 + r] = value;
  }

  Vec4 row(int i) const {
    return {fMat[i + 0], fMat[i + 4], fMat[i + 8], fMat[i + 12]};
  }
  Vec4 col(int i) const {
    return {fMat[i * 4 + 0], fMat[i * 4 + 1], fMat[i * 4 + 2], fMat[i * 4 + 3]};
  }

  void setRow(int i, const Vec4& v) {
    fMat[i + 0] = v.x;
    fMat[i + 4] = v.y;
    fMat[i + 8] = v.z;
    fMat[i + 12] = v.w;
  }
  void setCol(int i, const Vec4& v) {
    memcpy(&fMat[i * 4], v.ptr(), sizeof(v));
  }

  Matrix3D& setIdentity() {
    *this = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    return *this;
  }

  Matrix3D& setTranslate(float x, float y, float z = 0) {
    *this = {1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, z, 0, 0, 0, 1};
    return *this;
  }

  Matrix3D& setScale(float x, float y, float z = 1) {
    *this = {x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, 0, 0, 1};
    return *this;
  }

  /**
     *  Set this matrix to rotate about the specified unit-length axis vector,
     *  by an angle specified by its sin() and cos().
     *
     *  This does not attempt to verify that axis.length() == 1 or that the sin,cos values
     *  are correct.
   */
  Matrix3D& setRotateUnitSinCos(Vec3 axis, float sinAngle, float cosAngle);

  /**
     *  Set this matrix to rotate about the specified unit-length axis vector,
     *  by an angle specified in radians.
     *
     *  This does not attempt to verify that axis.length() == 1.
   */
  Matrix3D& setRotateUnit(Vec3 axis, float radians) {
    return this->setRotateUnitSinCos(axis, sin(radians), cos(radians));
  }

  /**
     *  Set this matrix to rotate about the specified axis vector,
     *  by an angle specified in radians.
     *
     *  Note: axis is not assumed to be unit-length, so it will be normalized internally.
     *        If axis is already unit-length, call setRotateAboutUnitRadians() instead.
   */
  Matrix3D& setRotate(Vec3 axis, float radians);

  Matrix3D& setConcat(const Matrix3D& a, const Matrix3D& b);

  friend Matrix3D operator*(const Matrix3D& a, const Matrix3D& b) {
    return Matrix3D(a, b);
  }

  Matrix3D& preConcat(const Matrix3D& m) {
    return this->setConcat(*this, m);
  }

  Matrix3D& postConcat(const Matrix3D& m) {
    return this->setConcat(m, *this);
  }

  /**
     *  A matrix is categorized as 'perspective' if the bottom row is not [0, 0, 0, 1].
     *  For most uses, a bottom row of [0, 0, 0, X] behaves like a non-perspective matrix, though
     *  it will be categorized as perspective. Calling normalizePerspective() will change the
     *  matrix such that, if its bottom row was [0, 0, 0, X], it will be changed to [0, 0, 0, 1]
     *  by scaling the rest of the matrix by 1/X.
     *
     *  | A B C D |    | A/X B/X C/X D/X |
     *  | E F G H | -> | E/X F/X G/X H/X |   for X != 0
     *  | I J K L |    | I/X J/X K/X L/X |
     *  | 0 0 0 X |    |  0   0   0   1  |
   */
  void normalizePerspective();

  /** Returns true if all elements of the matrix are finite. Returns false if any
      element is infinity, or NaN.

      @return  true if matrix has only finite elements
  */
  bool isFinite() const;

  /** If this is invertible, return that in inverse and return true. If it is
     *  not invertible, return false and leave the inverse parameter unchanged.
   */
  bool invert(Matrix3D* inverse) const;

  Matrix3D transpose() const;

  tgfx::Rect MapRect(const Matrix3D& m, const tgfx::Rect& src);

  ////////////

  Vec4 map(float x, float y, float z, float w) const;
  Vec4 operator*(const Vec4& v) const {
    return this->map(v.x, v.y, v.z, v.w);
  }
  Vec3 operator*(Vec3 v) const {
    auto v4 = this->map(v.x, v.y, v.z, 0);
    return {v4.x, v4.y, v4.z};
  }
  ////////////////////// Converting to/from Matrix

  /* When converting from Matrix3D to Matrix, the third row and
     * column is dropped.  When converting from Matrix to Matrix3D
     * the third row and column remain as identity:
     * [ a b c ]      [ a b 0 c ]
     * [ d e f ]  ->  [ d e 0 f ]
     * [ g h i ]      [ 0 0 1 0 ]
     *                [ g h 0 i ]
   */
  tgfx::Matrix asM33() const {
    return tgfx::Matrix::MakeAll(fMat[0], fMat[4], fMat[12], fMat[1], fMat[5], fMat[13], fMat[3],
                                 fMat[7], fMat[15]);
  }

  explicit Matrix3D(const tgfx::Matrix& src)
      : Matrix3D(src[0], src[1], 0, src[2], src[3], src[4], 0, src[5], 0, 0, 1, 0, src[6], src[7],
                 0, src[8]) {
  }

  Matrix3D& preTranslate(float x, float y, float z = 0);
  Matrix3D& postTranslate(float x, float y, float z = 0);

  Matrix3D& preScale(float x, float y);
  Matrix3D& preScale(float x, float y, float z);
  Matrix3D& preConcat(const tgfx::Matrix&);

 private:
  /* Stored in column-major.
     *  Indices
     *  0  4  8  12        1 0 0 trans_x
     *  1  5  9  13  e.g.  0 1 0 trans_y
     *  2  6 10  14        0 0 1 trans_z
     *  3  7 11  15        0 0 0 1
   */
  float fMat[16];
};
}  // namespace pag
