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
#include "pagx/runtime/MatrixDecompose.h"
#include "tgfx/core/Color.h"
#include "tgfx/core/Matrix.h"

namespace pagx {

// Linear interpolation helpers used by continuous-channel writers. Discrete channels ignore mix
// and overwrite directly, so they don't go through these helpers.
inline float MixFloat(float current, float target, float mix) {
  return current + (target - current) * mix;
}

// Mixes two perspective distances in the space the projection itself uses, the coefficient -1/d,
// instead of the distances. The projection tightens at a constant rate in the matrix that way, which
// is also how a browser interpolates the perspective() transform function: a distance from 1000 to
// 200 passes through 333 at the midpoint instead of 600.
//
// A distance of 0 means no projection, which is the coefficient 0, i.e. the d -> infinity end of
// that space. Folding it in keeps a segment touching 0 continuous: blending 0 with 200 halfway
// yields 400, not the near camera a linear distance blend would snap to on the first frame.
//
// Both distances are expected to be valid, i.e. non-negative; a negative one folds into the same
// "no projection" end, so callers that accept a distance from outside must reject it themselves
// before mixing.
inline float MixPerspectiveDistance(float current, float target, float mix) {
  double currentCoefficient = current > 0.0f ? 1.0 / static_cast<double>(current) : 0.0;
  double targetCoefficient = target > 0.0f ? 1.0 / static_cast<double>(target) : 0.0;
  double coefficient = currentCoefficient + (targetCoefficient - currentCoefficient) * mix;
  // A mix beyond either end (an overshooting easing) can push the coefficient past infinity; clamp
  // to no projection rather than continue into a negative, i.e. behind-the-camera, distance.
  if (!(coefficient > 0.0)) {
    return 0.0f;
  }
  return static_cast<float>(1.0 / coefficient);
}

inline tgfx::Color MixTGFXColor(const tgfx::Color& current, const tgfx::Color& target, float mix) {
  tgfx::Color result = current;
  result.red = MixFloat(current.red, target.red, mix);
  result.green = MixFloat(current.green, target.green, mix);
  result.blue = MixFloat(current.blue, target.blue, mix);
  result.alpha = MixFloat(current.alpha, target.alpha, mix);
  return result;
}

// Interpolates two 2D affine matrices by decomposing each into translate / rotation / scale / skew,
// mixing the components, and recomposing (see MatrixDecompose.h for the shared component math).
//
// Limitation: rotation is recovered via atan2, so winding (full turns) is lost and the mix takes
// the literal path between the recovered angles rather than the shortest arc. A tween crossing the
// +/-pi boundary (e.g. 170 deg to 190 deg) spins the long way through 0. Animations that need
// precise multi-turn or boundary-crossing rotation should drive a scalar rotation channel (e.g.
// Group::rotation) rather than a Layer matrix channel.
inline tgfx::Matrix MixTGFXMatrix(const tgfx::Matrix& current, const tgfx::Matrix& target,
                                  float mix) {
  float c[9];
  float t[9];
  current.get9(c);
  target.get9(t);
  // tgfx Matrix get9 layout: [scaleX skewX transX; skewY scaleY transY; 0 0 1]. DecomposeAffine
  // expects the row-major affine [a c tx; b d ty], i.e. a=scaleX, b=skewY, c=skewX, d=scaleY.
  auto dCurrent = DecomposeAffine(c[0], c[3], c[1], c[4], c[2], c[5]);
  auto dTarget = DecomposeAffine(t[0], t[3], t[1], t[4], t[2], t[5]);
  auto mixed = MixDecomposed(dCurrent, dTarget, mix);
  float ra = 0;
  float rb = 0;
  float rc = 0;
  float rd = 0;
  float rtx = 0;
  float rty = 0;
  RecomposeAffine(mixed, &ra, &rb, &rc, &rd, &rtx, &rty);
  return tgfx::Matrix::MakeAll(ra, rc, rtx, rb, rd, rty);
}

}  // namespace pagx
