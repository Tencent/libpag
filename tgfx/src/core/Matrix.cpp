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

#include "tgfx/core/Matrix.h"
#include <cfloat>
#include "core/utils/MathExtra.h"

namespace tgfx {

void Matrix::reset() {
  values[SCALE_X] = values[SCALE_Y] = values[PERSP_2] = 1;
  values[SKEW_X] = values[SKEW_Y] = values[TRANS_X] = values[TRANS_Y] = values[PERSP_0] =
      values[PERSP_1] = 0;
}

bool operator==(const Matrix& a, const Matrix& b) {
  const float* ma = a.values;
  const float* mb = b.values;
  return ma[0] == mb[0] && ma[1] == mb[1] && ma[2] == mb[2] && ma[3] == mb[3] && ma[4] == mb[4] &&
         ma[5] == mb[5] && ma[6] == mb[6] && ma[7] == mb[7] && ma[8] == mb[8];
}

void Matrix::setAll(float scaleX, float skewX, float transX, float skewY, float scaleY,
                    float transY, float persp0, float persp1, float persp2) {
  values[SCALE_X] = scaleX;
  values[SKEW_X] = skewX;
  values[TRANS_X] = transX;
  values[SKEW_Y] = skewY;
  values[SCALE_Y] = scaleY;
  values[TRANS_Y] = transY;
  values[PERSP_0] = persp0;
  values[PERSP_1] = persp1;
  values[PERSP_2] = persp2;
}

void Matrix::setAffine(float a, float b, float c, float d, float tx, float ty) {
  values[SCALE_X] = a;
  values[SKEW_X] = c;
  values[TRANS_X] = tx;
  values[SKEW_Y] = b;
  values[SCALE_Y] = d;
  values[TRANS_Y] = ty;
  values[PERSP_0] = 0;
  values[PERSP_1] = 0;
  values[PERSP_2] = 1;
}

void Matrix::setTranslate(float dx, float dy) {
  if ((dx != 0) | (dy != 0)) {
    values[TRANS_X] = dx;
    values[TRANS_Y] = dy;

    values[SCALE_X] = values[SCALE_Y] = values[PERSP_2] = 1;
    values[SKEW_X] = values[SKEW_Y] = values[PERSP_0] = values[PERSP_1] = 0;
  } else {
    this->reset();
  }
}

static inline float sdot(float a, float b, float c, float d) {
  return a * b + c * d;
}

void Matrix::preTranslate(float dx, float dy) {
  values[TRANS_X] += sdot(values[SCALE_X], dx, values[SKEW_X], dy);
  values[TRANS_Y] += sdot(values[SKEW_Y], dx, values[SCALE_Y], dy);
}

void Matrix::postTranslate(float dx, float dy) {
  values[TRANS_X] += dx;
  values[TRANS_Y] += dy;
}

void Matrix::setScaleTranslate(float sx, float sy, float tx, float ty) {
  values[SCALE_X] = sx;
  values[SKEW_X] = 0;
  values[TRANS_X] = tx;

  values[SKEW_Y] = 0;
  values[SCALE_Y] = sy;
  values[TRANS_Y] = ty;

  values[PERSP_0] = 0;
  values[PERSP_1] = 0;
  values[PERSP_2] = 1;
}

void Matrix::setScale(float sx, float sy, float px, float py) {
  if (1 == sx && 1 == sy) {
    this->reset();
  } else {
    values[SCALE_X] = sx;
    values[SKEW_X] = 0;
    values[TRANS_X] = 0;

    values[SKEW_Y] = 0;
    values[SCALE_Y] = sy;
    values[TRANS_Y] = 0;

    values[PERSP_0] = 0;
    values[PERSP_1] = 0;
    values[PERSP_2] = 1;
    this->setScaleTranslate(sx, sy, px - sx * px, py - sy * py);
  }
}

void Matrix::setScale(float sx, float sy) {
  if (1 == sx && 1 == sy) {
    this->reset();
  } else {
    values[SCALE_X] = sx;
    values[SCALE_Y] = sy;
    values[PERSP_2] = 1;

    values[TRANS_X] = values[TRANS_Y] = values[SKEW_X] = values[SKEW_Y] = values[PERSP_0] =
        values[PERSP_1] = 0;
  }
}

void Matrix::preScale(float sx, float sy, float px, float py) {
  if (1 == sx && 1 == sy) {
    return;
  }

  Matrix m = {};
  m.setScale(sx, sy, px, py);
  this->preConcat(m);
}

void Matrix::preScale(float sx, float sy) {
  if (1 == sx && 1 == sy) {
    return;
  }
  values[SCALE_X] *= sx;
  values[SKEW_Y] *= sx;
  values[PERSP_0] *= sx;

  values[SKEW_X] *= sy;
  values[SCALE_Y] *= sy;
  values[PERSP_1] *= sy;
}

void Matrix::postScale(float sx, float sy, float px, float py) {
  if (1 == sx && 1 == sy) {
    return;
  }
  Matrix m = {};
  m.setScale(sx, sy, px, py);
  this->postConcat(m);
}

void Matrix::postScale(float sx, float sy) {
  if (1 == sx && 1 == sy) {
    return;
  }
  Matrix m = {};
  m.setScale(sx, sy);
  this->postConcat(m);
}

void Matrix::setSinCos(float sinV, float cosV, float px, float py) {
  const float oneMinusCosV = 1 - cosV;

  values[SCALE_X] = cosV;
  values[SKEW_X] = -sinV;
  values[TRANS_X] = sdot(sinV, py, oneMinusCosV, px);

  values[SKEW_Y] = sinV;
  values[SCALE_Y] = cosV;
  values[TRANS_Y] = sdot(-sinV, px, oneMinusCosV, py);

  values[PERSP_0] = values[PERSP_1] = 0;
  values[PERSP_2] = 1;
}

void Matrix::setSinCos(float sinV, float cosV) {
  values[SCALE_X] = cosV;
  values[SKEW_X] = -sinV;
  values[TRANS_X] = 0;

  values[SKEW_Y] = sinV;
  values[SCALE_Y] = cosV;
  values[TRANS_Y] = 0;

  values[PERSP_0] = values[PERSP_1] = 0;
  values[PERSP_2] = 1;
}

void Matrix::setRotate(float degrees, float px, float py) {
  float rad = DegreesToRadians(degrees);
  this->setSinCos(SinSnapToZero(rad), CosSnapToZero(rad), px, py);
}

void Matrix::setRotate(float degrees) {
  float rad = DegreesToRadians(degrees);
  this->setSinCos(SinSnapToZero(rad), CosSnapToZero(rad));
}

void Matrix::preRotate(float degrees, float px, float py) {
  Matrix m = {};
  m.setRotate(degrees, px, py);
  this->preConcat(m);
}

void Matrix::preRotate(float degrees) {
  Matrix m = {};
  m.setRotate(degrees);
  this->preConcat(m);
}

void Matrix::postRotate(float degrees, float px, float py) {
  Matrix m = {};
  m.setRotate(degrees, px, py);
  this->postConcat(m);
}

void Matrix::postRotate(float degrees) {
  Matrix m = {};
  m.setRotate(degrees);
  this->postConcat(m);
}

void Matrix::setSkew(float sx, float sy, float px, float py) {
  values[SCALE_X] = 1;
  values[SKEW_X] = sx;
  values[TRANS_X] = -sx * py;

  values[SKEW_Y] = sy;
  values[SCALE_Y] = 1;
  values[TRANS_Y] = -sy * px;

  values[PERSP_0] = values[PERSP_1] = 0;
  values[PERSP_2] = 1;
}

void Matrix::setSkew(float sx, float sy) {
  values[SCALE_X] = 1;
  values[SKEW_X] = sx;
  values[TRANS_X] = 0;

  values[SKEW_Y] = sy;
  values[SCALE_Y] = 1;
  values[TRANS_Y] = 0;

  values[PERSP_0] = values[PERSP_1] = 0;
  values[PERSP_2] = 1;
}

void Matrix::preSkew(float sx, float sy, float px, float py) {
  Matrix m = {};
  m.setSkew(sx, sy, px, py);
  this->preConcat(m);
}

void Matrix::preSkew(float sx, float sy) {
  Matrix m = {};
  m.setSkew(sx, sy);
  this->preConcat(m);
}

void Matrix::postSkew(float sx, float sy, float px, float py) {
  Matrix m = {};
  m.setSkew(sx, sy, px, py);
  this->postConcat(m);
}

void Matrix::postSkew(float sx, float sy) {
  Matrix m = {};
  m.setSkew(sx, sy);
  this->postConcat(m);
}

void Matrix::setConcat(const Matrix& first, const Matrix& second) {
  auto& matA = first.values;
  auto& matB = second.values;
  auto a = matB[SCALE_X] * matA[SCALE_X];
  auto b = 0.0;
  auto c = 0.0;
  auto d = matB[SCALE_Y] * matA[SCALE_Y];
  auto tx = matB[TRANS_X] * matA[SCALE_X] + matA[TRANS_X];
  auto ty = matB[TRANS_Y] * matA[SCALE_Y] + matA[TRANS_Y];

  if (matB[SKEW_Y] != 0.0 || matB[SKEW_X] != 0.0 || matA[SKEW_Y] != 0.0 || matA[SKEW_X] != 0.0) {
    a += matB[SKEW_Y] * matA[SKEW_X];
    d += matB[SKEW_X] * matA[SKEW_Y];
    b += matB[SCALE_X] * matA[SKEW_Y] + matB[SKEW_Y] * matA[SCALE_Y];
    c += matB[SKEW_X] * matA[SCALE_X] + matB[SCALE_Y] * matA[SKEW_X];
    tx += matB[TRANS_Y] * matA[SKEW_X];
    ty += matB[TRANS_X] * matA[SKEW_Y];
  }

  values[SCALE_X] = a;
  values[SKEW_Y] = b;
  values[SKEW_X] = c;
  values[SCALE_Y] = d;
  values[TRANS_X] = tx;
  values[TRANS_Y] = ty;
  values[PERSP_0] = 0;
  values[PERSP_1] = 0;
  values[PERSP_2] = 1;
}

void Matrix::preConcat(const Matrix& mat) {
  // check for identity first, so we don't do a needless copy of ourselves
  // to ourselves inside setConcat()
  if (!mat.isIdentity()) {
    this->setConcat(*this, mat);
  }
}

void Matrix::postConcat(const Matrix& mat) {
  // check for identity first, so we don't do a needless copy of ourselves
  // to ourselves inside setConcat()
  if (!mat.isIdentity()) {
    this->setConcat(mat, *this);
  }
}

bool Matrix::invertible() const {
  float determinant = values[SCALE_X] * values[SCALE_Y] - values[SKEW_Y] * values[SKEW_X];
  return !(FloatNearlyZero(determinant, FLOAT_NEARLY_ZERO * FLOAT_NEARLY_ZERO * FLOAT_NEARLY_ZERO));
}

bool Matrix::invertNonIdentity(Matrix* inverse) const {
  auto a = values[SCALE_X];
  auto d = values[SCALE_Y];
  auto c = values[SKEW_X];
  auto b = values[SKEW_Y];
  auto tx = values[TRANS_X];
  auto ty = values[TRANS_Y];

  if (b == 0 && c == 0) {
    if (a == 0 || d == 0) {
      return false;
    }
    a = 1 / a;
    d = 1 / d;
    tx = -a * tx;
    ty = -d * ty;
    inverse->setAffine(a, b, c, d, tx, ty);
    return true;
  }
  float determinant = a * d - b * c;
  if (FloatNearlyZero(determinant, FLOAT_NEARLY_ZERO * FLOAT_NEARLY_ZERO * FLOAT_NEARLY_ZERO)) {
    return false;
  }
  determinant = 1 / determinant;
  a = d * determinant;
  b = -b * determinant;
  c = -c * determinant;
  d = values[SCALE_X] * determinant;
  tx = -(a * values[TRANS_X] + c * values[TRANS_Y]);
  ty = -(b * values[TRANS_X] + d * values[TRANS_Y]);
  inverse->setAffine(a, b, c, d, tx, ty);
  return true;
}

void Matrix::mapPoints(Point dst[], const Point src[], int count) const {
  auto tx = values[TRANS_X];
  auto ty = values[TRANS_Y];
  auto sx = values[SCALE_X];
  auto sy = values[SCALE_Y];
  auto kx = values[SKEW_X];
  auto ky = values[SKEW_Y];
  for (int i = 0; i < count; i++) {
    auto x = src[i].x * sx + src[i].y * kx + tx;
    auto y = src[i].x * ky + src[i].y * sy + ty;
    dst[i].set(x, y);
  }
}

void Matrix::mapXY(float x, float y, Point* result) const {
  auto tx = values[TRANS_X];
  auto ty = values[TRANS_Y];
  auto sx = values[SCALE_X];
  auto sy = values[SCALE_Y];
  auto kx = values[SKEW_X];
  auto ky = values[SKEW_Y];
  result->set(x * sx + y * kx + tx, x * ky + y * sy + ty);
}

void Matrix::mapRect(Rect* dst, const Rect& src) const {
  Point quad[4];
  quad[0].set(src.left, src.top);
  quad[1].set(src.right, src.top);
  quad[2].set(src.right, src.bottom);
  quad[3].set(src.left, src.bottom);
  mapPoints(quad, quad, 4);
  dst->setBounds(quad, 4);
}

float Matrix::getMinScale() const {
  float results[2];
  if (getMinMaxScaleFactors(results)) {
    return results[0];
  }
  return -1.0f;
}

float Matrix::getMaxScale() const {
  float results[2];
  if (getMinMaxScaleFactors(results)) {
    return results[1];
  }
  return -1.0f;
}

bool Matrix::getMinMaxScaleFactors(float* results) const {
  float a = sdot(values[SCALE_X], values[SCALE_X], values[SKEW_Y], values[SKEW_Y]);
  float b = sdot(values[SCALE_X], values[SKEW_X], values[SCALE_Y], values[SKEW_Y]);
  float c = sdot(values[SKEW_X], values[SKEW_X], values[SCALE_Y], values[SCALE_Y]);
  float bSqd = b * b;
  if (bSqd <= FLOAT_NEARLY_ZERO * FLOAT_NEARLY_ZERO) {
    results[0] = a;
    results[1] = c;
    if (results[0] > results[1]) {
      using std::swap;
      swap(results[0], results[1]);
    }
  } else {
    float aminusc = a - c;
    float apluscdiv2 = (a + c) * 0.5f;
    float x = sqrtf(aminusc * aminusc + 4 * bSqd) * 0.5f;
    results[0] = apluscdiv2 - x;
    results[1] = apluscdiv2 + x;
  }
  auto isFinite = (results[0] * 0 == 0);
  if (!isFinite) {
    return false;
  }
  if (results[0] < 0) {
    results[0] = 0;
  }
  results[0] = sqrtf(results[0]);
  isFinite = (results[1] * 0 == 0);
  if (!isFinite) {
    return false;
  }
  if (results[1] < 0) {
    results[1] = 0;
  }
  results[1] = sqrtf(results[1]);
  return true;
}

bool Matrix::isFinite() const {
  return FloatsAreFinite(values, 9);
}

const Matrix& Matrix::I() {
  static const Matrix identity = Matrix::MakeAll(1, 0, 0, 0, 1, 0, 0, 0, 1);
  return identity;
}
}  // namespace tgfx
