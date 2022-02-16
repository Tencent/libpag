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

#include "base/utils/TGFXCast.h"
#include "pag/types.h"

namespace pag {

void Matrix::reset() {
  ToTGFX(this)->reset();
}

bool operator==(const Matrix& a, const Matrix& b) {
  return ToTGFX(a) == ToTGFX(b);
}

void Matrix::setAll(float scaleX, float skewX, float transX, float skewY, float scaleY,
                    float transY, float persp0, float persp1, float persp2) {
  ToTGFX(this)->setAll(scaleX, skewX, transX, skewY, scaleY, transY, persp0, persp1, persp2);
}

void Matrix::setAffine(float a, float b, float c, float d, float tx, float ty) {
  ToTGFX(this)->setAffine(a, b, c, d, tx, ty);
}

void Matrix::setTranslate(float dx, float dy) {
  ToTGFX(this)->setTranslate(dx, dy);
}

void Matrix::preTranslate(float dx, float dy) {
  ToTGFX(this)->preTranslate(dx, dy);
}

void Matrix::postTranslate(float dx, float dy) {
  ToTGFX(this)->postTranslate(dx, dy);
}

void Matrix::setScale(float sx, float sy, float px, float py) {
  ToTGFX(this)->setScale(sx, sy, px, py);
}

void Matrix::setScale(float sx, float sy) {
  ToTGFX(this)->setScale(sx, sy);
}

void Matrix::preScale(float sx, float sy, float px, float py) {
  ToTGFX(this)->preScale(sx, sy, px, py);
}

void Matrix::preScale(float sx, float sy) {
  ToTGFX(this)->preScale(sx, sy);
}

void Matrix::postScale(float sx, float sy, float px, float py) {
  ToTGFX(this)->postScale(sx, sy, px, py);
}

void Matrix::postScale(float sx, float sy) {
  ToTGFX(this)->postScale(sx, sy);
}

void Matrix::setSinCos(float sinV, float cosV, float px, float py) {
  ToTGFX(this)->setSinCos(sinV, cosV, px, py);
}

void Matrix::setSinCos(float sinV, float cosV) {
  ToTGFX(this)->setSinCos(sinV, cosV);
}

void Matrix::setRotate(float degrees, float px, float py) {
  ToTGFX(this)->setRotate(degrees, px, py);
}

void Matrix::setRotate(float degrees) {
  ToTGFX(this)->setRotate(degrees);
}

void Matrix::preRotate(float degrees, float px, float py) {
  ToTGFX(this)->preRotate(degrees, px, py);
}

void Matrix::preRotate(float degrees) {
  ToTGFX(this)->preRotate(degrees);
}

void Matrix::postRotate(float degrees, float px, float py) {
  ToTGFX(this)->postRotate(degrees, px, py);
}

void Matrix::postRotate(float degrees) {
  ToTGFX(this)->postRotate(degrees);
}

void Matrix::setSkew(float sx, float sy, float px, float py) {
  ToTGFX(this)->setSkew(sx, sy, px, py);
}

void Matrix::setSkew(float sx, float sy) {
  ToTGFX(this)->setSkew(sx, sy);
}

void Matrix::preSkew(float sx, float sy, float px, float py) {
  ToTGFX(this)->preSkew(sx, sy, px, py);
}

void Matrix::preSkew(float sx, float sy) {
  ToTGFX(this)->preSkew(sx, sy);
}

void Matrix::postSkew(float sx, float sy, float px, float py) {
  ToTGFX(this)->postSkew(sx, sy, px, py);
}

void Matrix::postSkew(float sx, float sy) {
  ToTGFX(this)->postSkew(sx, sy);
}

void Matrix::setConcat(const Matrix& first, const Matrix& second) {
  ToTGFX(this)->setConcat(ToTGFX(first), ToTGFX(second));
}

void Matrix::preConcat(const Matrix& mat) {
  ToTGFX(this)->preConcat(ToTGFX(mat));
}

void Matrix::postConcat(const Matrix& mat) {
  ToTGFX(this)->postConcat(ToTGFX(mat));
}

bool Matrix::invert(Matrix* inverse) const {
  return ToTGFX(this)->invert(ToTGFX(inverse));
}

bool Matrix::invertible() const {
  return ToTGFX(this)->invertible();
}

void Matrix::mapPoints(Point dst[], const Point src[], int count) const {
  ToTGFX(this)->mapPoints(ToTGFX(dst), ToTGFX(src), count);
}

void Matrix::mapXY(float x, float y, Point* result) const {
  ToTGFX(this)->mapXY(x, y, ToTGFX(result));
}

void Matrix::mapRect(Rect* dst, const Rect& src) const {
  ToTGFX(this)->mapRect(ToTGFX(dst), ToTGFX(src));
}

float Matrix::getMinScale() const {
  return ToTGFX(this)->getMinScale();
}

float Matrix::getMaxScale() const {
  return ToTGFX(this)->getMaxScale();
}

bool Matrix::isFinite() const {
  return ToTGFX(this)->isFinite();
}

const Matrix& Matrix::I() {
  static const Matrix identity = Matrix::MakeAll(1, 0, 0, 0, 1, 0, 0, 0, 1);
  return identity;
}

const Matrix& Matrix::Invalid() {
  static const Matrix invalid = Matrix::MakeAll(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX,
                                                FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);
  return invalid;
}
}  // namespace pag