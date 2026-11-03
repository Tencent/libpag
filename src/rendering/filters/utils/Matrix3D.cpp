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

#include "Matrix3D.h"
#include <cstring>
#include "base/utils/MathUtil.h"

namespace pag {

static Vec3 NormalizeVector(const Vec3& v) {
  auto length = v.length();
  return FloatNearlyZero(length) ? v : v * (1.0f / length);
}

static float Invert4x4Matrix(const float inMatrix[16], float outMatrix[16]) {
  float a00 = inMatrix[0];
  float a01 = inMatrix[1];
  float a02 = inMatrix[2];
  float a03 = inMatrix[3];
  float a10 = inMatrix[4];
  float a11 = inMatrix[5];
  float a12 = inMatrix[6];
  float a13 = inMatrix[7];
  float a20 = inMatrix[8];
  float a21 = inMatrix[9];
  float a22 = inMatrix[10];
  float a23 = inMatrix[11];
  float a30 = inMatrix[12];
  float a31 = inMatrix[13];
  float a32 = inMatrix[14];
  float a33 = inMatrix[15];

  float b00 = a00 * a11 - a01 * a10;
  float b01 = a00 * a12 - a02 * a10;
  float b02 = a00 * a13 - a03 * a10;
  float b03 = a01 * a12 - a02 * a11;
  float b04 = a01 * a13 - a03 * a11;
  float b05 = a02 * a13 - a03 * a12;
  float b06 = a20 * a31 - a21 * a30;
  float b07 = a20 * a32 - a22 * a30;
  float b08 = a20 * a33 - a23 * a30;
  float b09 = a21 * a32 - a22 * a31;
  float b10 = a21 * a33 - a23 * a31;
  float b11 = a22 * a33 - a23 * a32;

  // Calculate the determinant
  float determinant = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
  if (outMatrix) {
    if (FloatNearlyZero(determinant, FLOAT_NEARLY_ZERO * FLOAT_NEARLY_ZERO * FLOAT_NEARLY_ZERO)) {
      return 0.0;
    }
    float invdet = 1.0 / determinant;
    b00 *= invdet;
    b01 *= invdet;
    b02 *= invdet;
    b03 *= invdet;
    b04 *= invdet;
    b05 *= invdet;
    b06 *= invdet;
    b07 *= invdet;
    b08 *= invdet;
    b09 *= invdet;
    b10 *= invdet;
    b11 *= invdet;

    outMatrix[0] = a11 * b11 - a12 * b10 + a13 * b09;
    outMatrix[1] = a02 * b10 - a01 * b11 - a03 * b09;
    outMatrix[2] = a31 * b05 - a32 * b04 + a33 * b03;
    outMatrix[3] = a22 * b04 - a21 * b05 - a23 * b03;
    outMatrix[4] = a12 * b08 - a10 * b11 - a13 * b07;
    outMatrix[5] = a00 * b11 - a02 * b08 + a03 * b07;
    outMatrix[6] = a32 * b02 - a30 * b05 - a33 * b01;
    outMatrix[7] = a20 * b05 - a22 * b02 + a23 * b01;
    outMatrix[8] = a10 * b10 - a11 * b08 + a13 * b06;
    outMatrix[9] = a01 * b08 - a00 * b10 - a03 * b06;
    outMatrix[10] = a30 * b04 - a31 * b02 + a33 * b00;
    outMatrix[11] = a21 * b02 - a20 * b04 - a23 * b00;
    outMatrix[12] = a11 * b07 - a10 * b09 - a12 * b06;
    outMatrix[13] = a00 * b09 - a01 * b07 + a02 * b06;
    outMatrix[14] = a31 * b01 - a30 * b03 - a32 * b00;
    outMatrix[15] = a20 * b03 - a21 * b01 + a22 * b00;

    // If 1/det overflows to infinity (i.e. det is denormalized) or any of the inverted matrix
    // values is non-finite, return zero to indicate a non-invertible matrix.
    if (!FloatsAreFinite(outMatrix, 16)) {
      determinant = 0.0f;
    }
  }
  return determinant;
}

Matrix3D Matrix3D::Scale(float x, float y, float z) {
  Matrix3D result = {};
  result.fMat[0] = x;
  result.fMat[5] = y;
  result.fMat[10] = z;
  return result;
}

Matrix3D Matrix3D::Rotate(Vec3 axis, float radians) {
  Matrix3D result = {};
  result.setRotate(axis, radians);
  return result;
}

Matrix3D Matrix3D::LookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
  auto forward = NormalizeVector(center - eye);
  auto upVector = NormalizeVector(up);
  auto side = NormalizeVector(forward.cross(upVector));
  auto sideUp = side.cross(forward);

  // The view matrix is the inverse of the camera basis matrix, whose columns are the side, up,
  // backward and eye vectors.
  Matrix3D camera = {};
  camera.fMat[0] = side.x;
  camera.fMat[1] = side.y;
  camera.fMat[2] = side.z;
  camera.fMat[4] = sideUp.x;
  camera.fMat[5] = sideUp.y;
  camera.fMat[6] = sideUp.z;
  camera.fMat[8] = -forward.x;
  camera.fMat[9] = -forward.y;
  camera.fMat[10] = -forward.z;
  camera.fMat[12] = eye.x;
  camera.fMat[13] = eye.y;
  camera.fMat[14] = eye.z;

  Matrix3D result = {};
  if (!camera.invert(&result)) {
    result.setIdentity();
  }
  return result;
}

Matrix3D Matrix3D::Perspective(float zNear, float zFar, float angle) {
  if (zFar <= zNear) {
    return Matrix3D();
  }

  float denomInv = 1.0f / (zFar - zNear);
  float halfAngle = angle * 0.5f;
  float cot = cosf(halfAngle) / sinf(halfAngle);

  Matrix3D result = {};
  result.setRC(0, 0, cot);
  result.setRC(1, 1, cot);
  result.setRC(2, 2, (zFar + zNear) * denomInv);
  result.setRC(2, 3, 2 * zFar * zNear * denomInv);
  result.setRC(3, 2, -1);
  return result;
}

Matrix3D& Matrix3D::setScale(float x, float y, float z) {
  *this = Matrix3D::Scale(x, y, z);
  return *this;
}

Matrix3D& Matrix3D::postTranslate(float x, float y, float z) {
  const float translate[4] = {x, y, z, 0.0f};
  float result[16];
  for (int i = 0; i < 4; i++) {
    result[i] = translate[i] * fMat[3] + fMat[i];
    result[4 + i] = translate[i] * fMat[7] + fMat[4 + i];
    result[8 + i] = translate[i] * fMat[11] + fMat[8 + i];
    result[12 + i] = translate[i] * fMat[15] + fMat[12 + i];
  }
  memcpy(fMat, result, sizeof(fMat));
  return *this;
}

Matrix3D& Matrix3D::postConcat(const Matrix3D& matrix) {
  return setConcat(matrix, *this);
}

bool Matrix3D::invert(Matrix3D* inverse) const {
  float result[16];
  if (inverse == nullptr || Invert4x4Matrix(fMat, result) == 0.0f) {
    return false;
  }
  memcpy(inverse->fMat, result, sizeof(result));
  return true;
}

void Matrix3D::getColMajor(float values[]) const {
  memcpy(values, fMat, sizeof(fMat));
}

Matrix3D& Matrix3D::setIdentity() {
  *this = Matrix3D();
  return *this;
}

Matrix3D& Matrix3D::setConcat(const Matrix3D& a, const Matrix3D& b) {
  float result[16];
  for (int column = 0; column < 4; column++) {
    for (int row = 0; row < 4; row++) {
      result[column * 4 + row] =
          a.fMat[row] * b.fMat[column * 4] +
          (a.fMat[4 + row] * b.fMat[column * 4 + 1] +
           (a.fMat[8 + row] * b.fMat[column * 4 + 2] + a.fMat[12 + row] * b.fMat[column * 4 + 3]));
    }
  }
  memcpy(fMat, result, sizeof(fMat));
  return *this;
}

Matrix3D& Matrix3D::setRotate(Vec3 axis, float radians) {
  auto length = axis.length();
  if (length > 0 && (length * 0 == 0)) {
    return setRotateUnitSinCos(axis * (1.0f / length), sin(radians), cos(radians));
  }
  return setIdentity();
}

Matrix3D& Matrix3D::setRotateUnitSinCos(Vec3 axis, float sinAngle, float cosAngle) {
  // Taken from "Essential Mathematics for Games and Interactive Applications"
  //             James M. Van Verth and Lars M. Bishop -- third edition
  float x = axis.x;
  float y = axis.y;
  float z = axis.z;
  float c = cosAngle;
  float s = sinAngle;
  float t = 1 - c;

  fMat[0] = t * x * x + c;
  fMat[4] = t * x * y - s * z;
  fMat[8] = t * x * z + s * y;
  fMat[12] = 0;
  fMat[1] = t * x * y + s * z;
  fMat[5] = t * y * y + c;
  fMat[9] = t * y * z - s * x;
  fMat[13] = 0;
  fMat[2] = t * x * z - s * y;
  fMat[6] = t * y * z + s * x;
  fMat[10] = t * z * z + c;
  fMat[14] = 0;
  fMat[3] = 0;
  fMat[7] = 0;
  fMat[11] = 0;
  fMat[15] = 1;
  return *this;
}

}  // namespace pag
