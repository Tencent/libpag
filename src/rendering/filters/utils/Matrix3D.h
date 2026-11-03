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

namespace pag {

struct Vec3 {
  float x = 0;
  float y = 0;
  float z = 0;

  Vec3 operator+(const Vec3& v) const {
    return {x + v.x, y + v.y, z + v.z};
  }

  Vec3 operator-(const Vec3& v) const {
    return {x - v.x, y - v.y, z - v.z};
  }

  Vec3 operator*(float scalar) const {
    return {x * scalar, y * scalar, z * scalar};
  }

  Vec3& operator+=(const Vec3& v) {
    x += v.x;
    y += v.y;
    z += v.z;
    return *this;
  }

  float length() const {
    return sqrtf(x * x + y * y + z * z);
  }

  Vec3 cross(const Vec3& v) const {
    return {y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x};
  }
};

/**
 * Matrix3D is a 4x4 matrix stored in column-major order. It assumes a right-handed coordinate
 * system where +X goes to the right, +Y goes down and +Z goes into the screen.
 */
class Matrix3D {
 public:
  Matrix3D() = default;

  /** Returns a matrix that scales by x, y and z along each axis. */
  static Matrix3D Scale(float x, float y, float z = 1);

  /** Returns a matrix that rotates by radians (in radians) around the given axis. */
  static Matrix3D Rotate(Vec3 axis, float radians);

  /** Returns a view matrix looking from eye toward center with the given up direction. */
  static Matrix3D LookAt(const Vec3& eye, const Vec3& center, const Vec3& up);

  /** Returns a perspective projection matrix; angle is the vertical field of view in radians and
   * zNear/zFar are the near/far clip plane distances. */
  static Matrix3D Perspective(float zNear, float zFar, float angle);

  /** Sets this matrix to a scale transform by x, y and z, replacing its current value. */
  Matrix3D& setScale(float x, float y, float z = 1);

  /** Post-translates this matrix by x, y and z. */
  Matrix3D& postTranslate(float x, float y, float z = 0);

  /** Post-concatenates the given matrix, setting this = matrix * this, following this class's
   * column-major convention. */
  Matrix3D& postConcat(const Matrix3D& matrix);

  /** Inverts this matrix into inverse and returns whether the matrix is invertible. */
  bool invert(Matrix3D* inverse) const;

  /** Writes the 16 matrix elements into values in column-major order; values must hold at least 16
   * floats. */
  void getColMajor(float values[]) const;

 private:
  Matrix3D& setIdentity();

  Matrix3D& setConcat(const Matrix3D& a, const Matrix3D& b);

  Matrix3D& setRotate(Vec3 axis, float radians);

  Matrix3D& setRotateUnitSinCos(Vec3 axis, float sinAngle, float cosAngle);

  void setRC(int row, int column, float value) {
    fMat[column * 4 + row] = value;
  }

  /* Stored in column-major.
   *  Indices
   *  0  4  8  12        1 0 0 trans_x
   *  1  5  9  13  e.g.  0 1 0 trans_y
   *  2  6 10  14        0 0 1 trans_z
   *  3  7 11  15        0 0 0 1
   */
  float fMat[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

}  // namespace pag
