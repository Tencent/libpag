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

namespace pagx {

// Decomposed form of a 2D affine matrix used for interpolation. Interpolating these components
// independently (rather than the raw matrix entries) keeps rotation angular and scale uniform so a
// matrix tween follows the natural translate/rotate/scale/skew path instead of shearing through
// intermediate non-orthogonal states.
struct DecomposedMatrix {
  float translateX = 0;
  float translateY = 0;
  float rotation = 0;  // radians
  float scaleX = 1;
  float scaleY = 1;
  float skew = 0;  // radians, horizontal shear after rotation
};

// Decomposes the linear part [a c; b d] plus translation (tx, ty) of a 2D affine matrix into
// translate / rotation / scale / skew via a QR-like decomposition: the first basis column gives
// rotation and scaleX, the shear of the second column gives skew, and the remaining magnitude gives
// scaleY. Element naming follows the row-major affine [a c tx; b d ty]. This is the single source of
// truth for matrix decomposition; callers adapt their own matrix type into these six elements.
//
// Limitation: rotation is recovered via atan2 and confined to (-pi, pi], so winding (full turns) is
// lost. Interpolating two decomposed matrices takes the literal path between the recovered angles
// rather than the shortest arc, so a tween crossing the +/-pi boundary spins the long way. This is
// inherent to baked matrices: the authored angle cannot be reconstructed from the matrix alone.
// Animations needing precise multi-turn or boundary-crossing rotation should drive a scalar rotation
// channel (e.g. Group::rotation) instead of a matrix channel.
inline DecomposedMatrix DecomposeAffine(float a, float b, float c, float d, float tx, float ty) {
  DecomposedMatrix out = {};
  out.translateX = tx;
  out.translateY = ty;
  out.scaleX = std::sqrt(a * a + b * b);
  out.rotation = std::atan2(b, a);
  float cosR = std::cos(out.rotation);
  float sinR = std::sin(out.rotation);
  float shearedC = cosR * c + sinR * d;
  float scaleY = -sinR * c + cosR * d;
  out.scaleY = scaleY;
  out.skew = scaleY != 0.0f ? std::atan(shearedC / scaleY) : 0.0f;
  return out;
}

// Recomposes the six affine elements [a c tx; b d ty] from decomposed components, inverting
// DecomposeAffine. Outputs are written through the provided pointers (any may be null to skip).
inline void RecomposeAffine(const DecomposedMatrix& m, float* a, float* b, float* c, float* d,
                            float* tx, float* ty) {
  float cosR = std::cos(m.rotation);
  float sinR = std::sin(m.rotation);
  float tanSkew = std::tan(m.skew);
  // Linear part = Rotation * Skew * Scale, matching the decomposition order.
  if (a != nullptr) {
    *a = cosR * m.scaleX;
  }
  if (b != nullptr) {
    *b = sinR * m.scaleX;
  }
  if (c != nullptr) {
    *c = (cosR * tanSkew - sinR) * m.scaleY;
  }
  if (d != nullptr) {
    *d = (sinR * tanSkew + cosR) * m.scaleY;
  }
  if (tx != nullptr) {
    *tx = m.translateX;
  }
  if (ty != nullptr) {
    *ty = m.translateY;
  }
}

// Interpolates two decomposed matrices component-wise at fraction t in [0, 1].
inline DecomposedMatrix MixDecomposed(const DecomposedMatrix& a, const DecomposedMatrix& b,
                                      float t) {
  DecomposedMatrix out = {};
  out.translateX = a.translateX + (b.translateX - a.translateX) * t;
  out.translateY = a.translateY + (b.translateY - a.translateY) * t;
  out.rotation = a.rotation + (b.rotation - a.rotation) * t;
  out.scaleX = a.scaleX + (b.scaleX - a.scaleX) * t;
  out.scaleY = a.scaleY + (b.scaleY - a.scaleY) * t;
  out.skew = a.skew + (b.skew - a.skew) * t;
  return out;
}

}  // namespace pagx
