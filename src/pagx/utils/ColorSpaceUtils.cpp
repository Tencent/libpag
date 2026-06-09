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

#include "pagx/utils/ColorSpaceUtils.h"
#include <algorithm>
#include <cmath>
#include "tgfx/core/ColorSpace.h"

namespace pagx {

static const tgfx::ColorSpace* TGFXColorSpace(ColorSpace space) {
  if (space == ColorSpace::DisplayP3) {
    return tgfx::ColorSpace::DisplayP3().get();
  }
  return tgfx::ColorSpace::SRGB().get();
}

// Evaluates the ICC 7-parameter parametric (sRGBish) transfer function, matching skcms'
// skcms_TransferFunction_eval for the sRGBish branch. SRGB and DisplayP3 both use sRGBish transfer
// functions, so this single form covers every color space pagx produces. The sign handling lets it
// operate on out-of-gamut components that may go slightly negative after the gamut transform.
static float EvalTransferFunction(const tgfx::TransferFunction& fn, float x) {
  float sign = x < 0.0f ? -1.0f : 1.0f;
  x *= sign;
  float value = x < fn.d ? fn.c * x + fn.f : std::pow(fn.a * x + fn.b, fn.g) + fn.e;
  return sign * value;
}

Color ConvertColorSpace(const Color& color, ColorSpace target) {
  if (color.colorSpace == target) {
    return color;
  }
  const tgfx::ColorSpace* srcSpace = TGFXColorSpace(color.colorSpace);
  const tgfx::ColorSpace* dstSpace = TGFXColorSpace(target);
  // The gamut transform is a linear-light operation, so decode the gamma-encoded source components
  // to linear, apply the gamut matrix, then re-encode into the destination transfer function. A
  // bare matrix multiply on encoded values (as the previous implementation did) produces a visible
  // color shift.
  tgfx::TransferFunction srcTransfer = srcSpace->transferFunction();
  float linear[3] = {EvalTransferFunction(srcTransfer, color.red),
                     EvalTransferFunction(srcTransfer, color.green),
                     EvalTransferFunction(srcTransfer, color.blue)};
  tgfx::ColorMatrix33 matrix = {};
  srcSpace->gamutTransformTo(dstSpace, &matrix);
  float transformed[3] = {};
  for (int i = 0; i < 3; ++i) {
    transformed[i] = linear[0] * matrix.values[i][0] + linear[1] * matrix.values[i][1] +
                     linear[2] * matrix.values[i][2];
  }
  tgfx::TransferFunction dstInverse = dstSpace->inverseTransferFunction();
  Color result = color;
  result.red = std::clamp(EvalTransferFunction(dstInverse, transformed[0]), 0.0f, 1.0f);
  result.green = std::clamp(EvalTransferFunction(dstInverse, transformed[1]), 0.0f, 1.0f);
  result.blue = std::clamp(EvalTransferFunction(dstInverse, transformed[2]), 0.0f, 1.0f);
  result.colorSpace = target;
  return result;
}

static float LerpComponent(float a, float b, double t) {
  return static_cast<float>(a + (static_cast<double>(b) - a) * t);
}

Color LerpColor(const Color& a, const Color& b, double t) {
  // Bezier easing factors may overshoot below 0 or above 1; Color components are defined in
  // [0, 1], so clamp here rather than producing out-of-range channels.
  double clamped = std::clamp(t, 0.0, 1.0);
  Color end = a.colorSpace == b.colorSpace ? b : ConvertColorSpace(b, a.colorSpace);
  Color result = {};
  result.red = LerpComponent(a.red, end.red, clamped);
  result.green = LerpComponent(a.green, end.green, clamped);
  result.blue = LerpComponent(a.blue, end.blue, clamped);
  result.alpha = LerpComponent(a.alpha, end.alpha, clamped);
  result.colorSpace = a.colorSpace;
  return result;
}

}  // namespace pagx
