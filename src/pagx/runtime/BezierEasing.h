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

namespace pagx {

/**
 * Solves the cubic-bezier easing function defined by control points P1=(p1x,p1y) and P2=(p2x,p2y)
 * with implicit endpoints P0=(0,0) and P3=(1,1).
 *
 * Given an input progress in [0,1] representing horizontal time progress, returns the eased
 * progress (vertical) at that point. Implementation uses Newton-Raphson with bisection fallback;
 * accuracy is well below the millisecond range needed for keyframe interpolation.
 *
 * Inputs are clamped to [0,1] before solving; the returned value is also clamped to [0,1].
 */
double SolveBezierEasing(double p1x, double p1y, double p2x, double p2y, double input);

}  // namespace pagx
