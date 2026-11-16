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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/runtime/BezierEasing.h"
#include "base/utils/BezierEasing.h"
#include "pag/types.h"

namespace pagx {

// The solver delegates to the legacy pag::BezierEasing (BezierPath polyline flattening at
// precision 0.005, cached by control points). Sharing one implementation keeps migrated
// keyframe easing numerically identical to the legacy SingleEaseKeyframe evaluation: a separate
// higher-precision Newton solver here drifted by up to 3e-3 in eased progress, which
// large-displacement animations (ZC2's ~1700px mask morphs) amplified into 1-6px boundary
// offsets.
double SolveBezierEasing(double p1x, double p1y, double p2x, double p2y, double input) {
  if (input <= 0.0) {
    return 0.0;
  }
  if (input >= 1.0) {
    return 1.0;
  }
  pag::BezierEasing easing(pag::Point::Make(static_cast<float>(p1x), static_cast<float>(p1y)),
                           pag::Point::Make(static_cast<float>(p2x), static_cast<float>(p2y)));
  // CSS cubic-bezier allows y control points outside [0, 1] to express overshoot/anticipation;
  // the output is intentionally NOT clamped - value-type Lerp implementations decide how to
  // handle t outside [0, 1] (float lerp produces the overshoot, LerpColor clamps).
  return static_cast<double>(easing.getInterpolation(static_cast<float>(input)));
}

}  // namespace pagx
