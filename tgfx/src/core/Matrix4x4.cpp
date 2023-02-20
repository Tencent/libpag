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

#include "tgfx/core/Matrix4x4.h"
#include <cfloat>
#include "core/utils/MathExtra.h"
#include "include/private/SkVx.h"

namespace tgfx {

bool Matrix4x4::operator==(const Matrix4x4& other) const {
  if (this == &other) {
    return true;
  }
  
  auto a0 = pk::skvx::float4::Load(fMat +  0);
  auto a1 = pk::skvx::float4::Load(fMat +  4);
  auto a2 = pk::skvx::float4::Load(fMat +  8);
  auto a3 = pk::skvx::float4::Load(fMat + 12);

  auto b0 = pk::skvx::float4::Load(other.fMat +  0);
  auto b1 = pk::skvx::float4::Load(other.fMat +  4);
  auto b2 = pk::skvx::float4::Load(other.fMat +  8);
  auto b3 = pk::skvx::float4::Load(other.fMat + 12);

  auto eq = (a0 == b0) & (a1 == b1) & (a2 == b2) & (a3 == b3);
  return (eq[0] & eq[1] & eq[2] & eq[3]) == ~0;
}

static void transpose_arrays(float dst[], const float src[]) {
  dst[0]  = src[0]; dst[1]  = src[4]; dst[2]  = src[8];  dst[3]  = src[12];
  dst[4]  = src[1]; dst[5]  = src[5]; dst[6]  = src[9];  dst[7]  = src[13];
  dst[8]  = src[2]; dst[9]  = src[6]; dst[10] = src[10]; dst[11] = src[14];
  dst[12] = src[3]; dst[13] = src[7]; dst[14] = src[11]; dst[15] = src[15];
}

void Matrix4x4::getRowMajor(float v[]) const {
  transpose_arrays(v, fMat);
}

Matrix4x4& Matrix4x4::setConcat(const Matrix4x4& a, const Matrix4x4& b) {
  auto c0 = pk::skvx::float4::Load(a.fMat +  0);
  auto c1 = pk::skvx::float4::Load(a.fMat +  4);
  auto c2 = pk::skvx::float4::Load(a.fMat +  8);
  auto c3 = pk::skvx::float4::Load(a.fMat + 12);

  auto compute = [&](pk::skvx::float4 r) {
    return c0*r[0] + (c1*r[1] + (c2*r[2] + c3*r[3]));
  };

  auto m0 = compute(pk::skvx::float4::Load(b.fMat +  0));
  auto m1 = compute(pk::skvx::float4::Load(b.fMat +  4));
  auto m2 = compute(pk::skvx::float4::Load(b.fMat +  8));
  auto m3 = compute(pk::skvx::float4::Load(b.fMat + 12));

  m0.store(fMat +  0);
  m1.store(fMat +  4);
  m2.store(fMat +  8);
  m3.store(fMat + 12);
  return *this;
}

Matrix4x4& Matrix4x4::preConcat(const Matrix& b) {
  auto c0 = pk::skvx::float4::Load(fMat +  0);
  auto c1 = pk::skvx::float4::Load(fMat +  4);
  auto c3 = pk::skvx::float4::Load(fMat + 12);

  auto compute = [&](float r0, float r1, float r3) {
    return (c0*r0 + (c1*r1 + c3*r3));
  };

  auto m0 = compute(b[0], b[3], b[6]);
  auto m1 = compute(b[1], b[4], b[7]);
  auto m3 = compute(b[2], b[5], b[8]);

  m0.store(fMat +  0);
  m1.store(fMat +  4);
  m3.store(fMat + 12);
  return *this;
}

Matrix4x4& Matrix4x4::preTranslate(float x, float y, float z) {
  auto c0 = pk::skvx::float4::Load(fMat +  0);
  auto c1 = pk::skvx::float4::Load(fMat +  4);
  auto c2 = pk::skvx::float4::Load(fMat +  8);
  auto c3 = pk::skvx::float4::Load(fMat + 12);

  // only need to update the last column
  (c0*x + (c1*y + (c2*z + c3))).store(fMat + 12);
  return *this;
}

Matrix4x4& Matrix4x4::postTranslate(float x, float y, float z) {
  pk::skvx::float4 t = { x, y, z, 0 };
  (t * fMat[ 3] + pk::skvx::float4::Load(fMat +  0)).store(fMat +  0);
  (t * fMat[ 7] + pk::skvx::float4::Load(fMat +  4)).store(fMat +  4);
  (t * fMat[11] + pk::skvx::float4::Load(fMat +  8)).store(fMat +  8);
  (t * fMat[15] + pk::skvx::float4::Load(fMat + 12)).store(fMat + 12);
  return *this;
}

Matrix4x4& Matrix4x4::preScale(float x, float y) {
  auto c0 = pk::skvx::float4::Load(fMat +  0);
  auto c1 = pk::skvx::float4::Load(fMat +  4);

  (c0 * x).store(fMat + 0);
  (c1 * y).store(fMat + 4);
  return *this;
}

Matrix4x4& Matrix4x4::preScale(float x, float y, float z) {
  auto c0 = pk::skvx::float4::Load(fMat +  0);
  auto c1 = pk::skvx::float4::Load(fMat +  4);
  auto c2 = pk::skvx::float4::Load(fMat +  8);

  (c0 * x).store(fMat + 0);
  (c1 * y).store(fMat + 4);
  (c2 * z).store(fMat + 8);
  return *this;
}

SkV4 Matrix4x4::map(float x, float y, float z, float w) const {
  auto c0 = pk::skvx::float4::Load(fMat +  0);
  auto c1 = pk::skvx::float4::Load(fMat +  4);
  auto c2 = pk::skvx::float4::Load(fMat +  8);
  auto c3 = pk::skvx::float4::Load(fMat + 12);

  SkV4 v;
  (c0*x + (c1*y + (c2*z + c3*w))).store(&v.x);
  return v;
}

static Rect map_rect_affine(const Rect& src, const float mat[16]) {
  // When multiplied against vectors of the form <x,y,x,y>, 'flip' allows a single min()
  // to compute both the min and "negated" max between the xy coordinates. Once finished, another
  // multiplication produces the original max.
  const pk::skvx::float4 flip{1.f, 1.f, -1.f, -1.f};

  // Since z = 0 and it's assumed ther's no perspective, only load the upper 2x2 and (tx,ty) in c3
  auto c0 = pk::skvx::shuffle<0,1,0,1>(pk::skvx::float2::Load(mat + 0)) * flip;
  auto c1 = pk::skvx::shuffle<0,1,0,1>(pk::skvx::float2::Load(mat + 4)) * flip;
  auto c3 = pk::skvx::shuffle<0,1,0,1>(pk::skvx::float2::Load(mat + 12));

  // Compute the min and max of the four transformed corners pre-translation; then translate once
  // at the end.
  auto minMax = c3 + flip * min(min(c0 * src.left  + c1 * src.top,
                                    c0 * src.right + c1 * src.top),
                                min(c0 * src.left  + c1 * src.bottom,
                                    c0 * src.right + c1 * src.bottom));

  // minMax holds (min x, min y, max x, max y) so can be copied into an Rect expecting l,t,r,b
  Rect r;
  minMax.store(&r);
  return r;
}

static Rect map_rect_perspective(const Rect& src, const float mat[16]) {
  // Like map_rect_affine, z = 0 so we can skip the 3rd column, but we do need to compute w's
  // for each corner of the src rect.
  auto c0 = pk::skvx::float4::Load(mat + 0);
  auto c1 = pk::skvx::float4::Load(mat + 4);
  auto c3 = pk::skvx::float4::Load(mat + 12);

  // Unlike map_rect_affine, we do not defer the 4th column since we may need to homogeneous
  // coordinates to clip against the w=0 plane
  auto tl = c0 * src.left  + c1 * src.top    + c3;
  auto tr = c0 * src.right + c1 * src.top    + c3;
  auto bl = c0 * src.left  + c1 * src.bottom + c3;
  auto br = c0 * src.right + c1 * src.bottom + c3;

  // After clipping to w>0 and projecting to 2d, 'project' employs the same negation trick to
  // compute min and max at the same time.
  const pk::skvx::float4 flip{1.f, 1.f, -1.f, -1.f};
  auto project = [&flip](const pk::skvx::float4& p0, const pk::skvx::float4& p1, const pk::skvx::float4& p2) {
    float w0 = p0[3];
    float w0PlaneDistance = 1.f / (1 << 14);
    if (w0 >= w0PlaneDistance) {
      // Unclipped, just divide by w
      return flip * pk::skvx::shuffle<0,1,0,1>(p0) / w0;
    } else {
      auto clip = [&](const pk::skvx::float4& p) {
        float w = p[3];
        if (w >= w0PlaneDistance) {
          float t = (w0PlaneDistance - w0) / (w - w0);
          auto c = (t * pk::skvx::shuffle<0,1>(p) + (1.f - t) * pk::skvx::shuffle<0,1>(p0)) /
                   w0PlaneDistance;

          return flip * pk::skvx::shuffle<0,1,0,1>(c);
        } else {
          return pk::skvx::float4(std::numeric_limits<float>::infinity());
        }
      };
      // Clip both edges leaving p0, and return the min/max of the two clipped points
      // (since clip returns infinity when both p0 and 2nd vertex have w<0, it'll
      // automatically be ignored).
      return min(clip(p1), clip(p2));
    }
  };

  // Project all 4 corners, and pass in their adjacent vertices for clipping if it has w < 0,
  // then accumulate the min and max xy's.
  auto minMax = flip * min(min(project(tl, tr, bl), project(tr, br, tl)),
                           min(project(br, bl, tr), project(bl, tl, br)));

  Rect r;
  minMax.store(&r);
  return r;
}

Rect Matrix4x4::MapRect(const Matrix4x4& m, const Rect& src) {
  const bool hasPerspective =
      m.fMat[3] != 0 || m.fMat[7] != 0 || m.fMat[11] != 0 || m.fMat[15] != 1;
  if (hasPerspective) {
    return map_rect_perspective(src, m.fMat);
  } else {
    return map_rect_affine(src, m.fMat);
  }
}

void Matrix4x4::normalizePerspective() {
  // If the bottom row of the matrix is [0, 0, 0, not_one], we will treat the matrix as if it
  // is in perspective, even though it stills behaves like its affine. If we divide everything
  // by the not_one value, then it will behave the same, but will be treated as affine,
  // and therefore faster (e.g. clients can forward-difference calculations).
  if (fMat[15] != 1 && fMat[15] != 0 && fMat[3] == 0 && fMat[7] == 0 && fMat[11] == 0) {
    float inv = 1.0 / fMat[15];
    (pk::skvx::float4::Load(fMat +  0) * inv).store(fMat +  0);
    (pk::skvx::float4::Load(fMat +  4) * inv).store(fMat +  4);
    (pk::skvx::float4::Load(fMat +  8) * inv).store(fMat +  8);
    (pk::skvx::float4::Load(fMat + 12) * inv).store(fMat + 12);
    fMat[15] = 1.0f;
  }
}

///////////////////////////////////////////////////////////////////////////////

bool Matrix4x4::isFinite() const {
  return FloatsAreFinite(fMat, 16);
}

float Invert4x4Matrix(const float inMatrix[16], float outMatrix[16]) {
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

    outMatrix[0]  = a11 * b11 - a12 * b10 + a13 * b09;
    outMatrix[1]  = a02 * b10 - a01 * b11 - a03 * b09;
    outMatrix[2]  = a31 * b05 - a32 * b04 + a33 * b03;
    outMatrix[3]  = a22 * b04 - a21 * b05 - a23 * b03;
    outMatrix[4]  = a12 * b08 - a10 * b11 - a13 * b07;
    outMatrix[5]  = a00 * b11 - a02 * b08 + a03 * b07;
    outMatrix[6]  = a32 * b02 - a30 * b05 - a33 * b01;
    outMatrix[7]  = a20 * b05 - a22 * b02 + a23 * b01;
    outMatrix[8]  = a10 * b10 - a11 * b08 + a13 * b06;
    outMatrix[9]  = a01 * b08 - a00 * b10 - a03 * b06;
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

bool Matrix4x4::invert(Matrix4x4* inverse) const {
  float tmp[16];
  if (Invert4x4Matrix(fMat, tmp) == 0.0f) {
    return false;
  }
  memcpy(inverse->fMat, tmp, sizeof(tmp));
  return true;
}

Matrix4x4 Matrix4x4::transpose() const {
  Matrix4x4 trans(Matrix4x4::kUninitialized_Constructor);
  transpose_arrays(trans.fMat, fMat);
  return trans;
}

Matrix4x4& Matrix4x4::setRotateUnitSinCos(SkV3 axis, float sinAngle, float cosAngle) {
  // Taken from "Essential Mathematics for Games and Interactive Applications"
  //             James M. Van Verth and Lars M. Bishop -- third edition
  float x = axis.x;
  float y = axis.y;
  float z = axis.z;
  float c = cosAngle;
  float s = sinAngle;
  float t = 1 - c;

  *this = { t*x*x + c,   t*x*y - s*z, t*x*z + s*y, 0,
            t*x*y + s*z, t*y*y + c,   t*y*z - s*x, 0,
            t*x*z - s*y, t*y*z + s*x, t*z*z + c,   0,
            0,           0,           0,           1 };
  return *this;
}

Matrix4x4& Matrix4x4::setRotate(SkV3 axis, float radians) {
  float len = axis.length();
  if (len > 0 && (len * 0 == 0)) {
    this->setRotateUnit(axis * (1.0f / len), radians);
  } else {
    this->setIdentity();
  }
  return *this;
}

Matrix4x4 Matrix4x4::RectToRect(const Rect& src, const Rect& dst) {
  if (src.isEmpty()) {
    return Matrix4x4();
  } else if (dst.isEmpty()) {
    return Matrix4x4::Scale(0.f, 0.f, 0.f);
  }

  float sx = dst.width()  / src.width();
  float sy = dst.height() / src.height();

  float tx = dst.left - sx * src.left;
  float ty = dst.top  - sy * src.top;

  return Matrix4x4{sx,  0.f, 0.f, tx,
               0.f, sy,  0.f, ty,
               0.f, 0.f, 1.f, 0.f,
               0.f, 0.f, 0.f, 1.f};
}

static SkV3 normalize(SkV3 v) {
  const auto vlen = v.length();

  return FloatNearlyZero(vlen) ? v : v * (1.0f / vlen);
}

static SkV4 v4(SkV3 v, float w) { return {v.x, v.y, v.z, w}; }

Matrix4x4 Matrix4x4::LookAt(const SkV3& eye, const SkV3& center, const SkV3& up) {
  SkV3 f = normalize(center - eye);
  SkV3 u = normalize(up);
  SkV3 s = normalize(f.cross(u));

  Matrix4x4 m(Matrix4x4::kUninitialized_Constructor);
  if (!Matrix4x4::Cols(v4(s, 0), v4(s.cross(f), 0), v4(-f, 0), v4(eye, 1)).invert(&m)) {
    m.setIdentity();
  }
  return m;
}

Matrix4x4 Matrix4x4::Perspective(float near, float far, float angle) {
  if (far <= near) {
    return Matrix4x4();
  }

  float denomInv = 1.0f / (far - near);
  float halfAngle = angle * 0.5f;
  float cot = cosf(halfAngle) / sinf(halfAngle);

  Matrix4x4 m;
  m.setRC(0, 0, cot);
  m.setRC(1, 1, cot);
  m.setRC(2, 2, (far + near) * denomInv);
  m.setRC(2, 3, 2 * far * near * denomInv);
  m.setRC(3, 2, -1);
  return m;
}
}  // namespace tgfx
