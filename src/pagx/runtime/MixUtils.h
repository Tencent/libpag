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
#include "tgfx/core/Color.h"
#include "tgfx/core/Matrix.h"

namespace pagx {

// Linear interpolation helpers used by continuous-channel writers. Discrete channels ignore mix
// and overwrite directly, so they don't go through these helpers.
inline float MixFloat(float current, float target, float mix) {
  return current + (target - current) * mix;
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
// mixing the components, and recomposing. This keeps rotation angular and scale uniform so a matrix
// tween follows the natural transform path instead of shearing through non-orthogonal states.
inline tgfx::Matrix MixTGFXMatrix(const tgfx::Matrix& current, const tgfx::Matrix& target,
                                  float mix) {
  float c[9];
  float t[9];
  current.get9(c);
  target.get9(t);
  // tgfx Matrix get9 layout: [scaleX skewX transX; skewY scaleY transY; 0 0 1].
  float ca = c[0];  // scaleX
  float cc = c[1];  // skewX
  float ctx = c[2];
  float cb = c[3];  // skewY
  float cd = c[4];  // scaleY
  float cty = c[5];
  float ta = t[0];
  float tc = t[1];
  float ttx = t[2];
  float tb = t[3];
  float td = t[4];
  float tty = t[5];

  float cScaleX = std::sqrt(ca * ca + cb * cb);
  float cRot = std::atan2(cb, ca);
  float cCos = std::cos(cRot);
  float cSin = std::sin(cRot);
  float cShearC = cCos * cc + cSin * cd;
  float cScaleY = -cSin * cc + cCos * cd;
  float cSkew = cScaleY != 0.0f ? std::atan(cShearC / cScaleY) : 0.0f;

  float tScaleX = std::sqrt(ta * ta + tb * tb);
  float tRot = std::atan2(tb, ta);
  float tCos = std::cos(tRot);
  float tSin = std::sin(tRot);
  float tShearC = tCos * tc + tSin * td;
  float tScaleY = -tSin * tc + tCos * td;
  float tSkew = tScaleY != 0.0f ? std::atan(tShearC / tScaleY) : 0.0f;

  float mScaleX = MixFloat(cScaleX, tScaleX, mix);
  float mScaleY = MixFloat(cScaleY, tScaleY, mix);
  float mRot = MixFloat(cRot, tRot, mix);
  float mSkew = MixFloat(cSkew, tSkew, mix);
  float mTx = MixFloat(ctx, ttx, mix);
  float mTy = MixFloat(cty, tty, mix);

  float mCos = std::cos(mRot);
  float mSin = std::sin(mRot);
  float mTan = std::tan(mSkew);
  float ra = mCos * mScaleX;
  float rb = mSin * mScaleX;
  float rc = (mCos * mTan - mSin) * mScaleY;
  float rd = (mSin * mTan + mCos) * mScaleY;
  return tgfx::Matrix::MakeAll(ra, rc, mTx, rb, rd, mTy);
}

}  // namespace pagx
