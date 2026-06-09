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

#include "pagx/runtime/BezierEasing.h"
#include <algorithm>
#include <cmath>

namespace pagx {

// Cubic Bezier in one dimension: B(t) = 3*(1-t)^2*t*p1 + 3*(1-t)*t^2*p2 + t^3.
// Endpoints are implicit (0 at t=0, 1 at t=1) so the formula simplifies once expanded.

struct BezierCoefficients {
  double cx0;
  double cx1;
  double cx2;
};

static BezierCoefficients ComputeBezierCoefficients(double p1, double p2) {
  double cx0 = 3.0 * p1;
  double cx1 = 3.0 * (p2 - p1) - cx0;
  double cx2 = 1.0 - cx0 - cx1;
  return {cx0, cx1, cx2};
}

static double SampleCurve(double p1, double p2, double t) {
  // Coefficients of the polynomial form, derived by expanding the Bernstein form.
  auto coeff = ComputeBezierCoefficients(p1, p2);
  return ((coeff.cx2 * t + coeff.cx1) * t + coeff.cx0) * t;
}

static double SampleDerivative(double p1, double p2, double t) {
  auto coeff = ComputeBezierCoefficients(p1, p2);
  return (3.0 * coeff.cx2 * t + 2.0 * coeff.cx1) * t + coeff.cx0;
}

// Solves x(t) == input for t using Newton-Raphson, falling back to bisection when the derivative
// is too small to converge or iterations exceed the budget.
static double SolveCurveX(double p1x, double p2x, double input) {
  constexpr int NEWTON_ITERATIONS = 8;
  constexpr int MAX_BISECTION_ITERATIONS = 60;
  constexpr double EPSILON = 1.0e-7;

  double t = input;
  for (int i = 0; i < NEWTON_ITERATIONS; ++i) {
    double currentX = SampleCurve(p1x, p2x, t) - input;
    if (std::fabs(currentX) < EPSILON) {
      return t;
    }
    double derivative = SampleDerivative(p1x, p2x, t);
    if (std::fabs(derivative) < EPSILON) {
      break;
    }
    t -= currentX / derivative;
  }

  // Bisection fallback within [0, 1]. The iteration cap guards against non-convergence on
  // malformed control points (e.g. x outside [0, 1] yielding a non-monotonic x(t)), where the
  // midpoint can round onto a bound and make no further progress; 60 steps is well beyond what
  // double precision can resolve.
  double low = 0.0;
  double high = 1.0;
  t = input;
  for (int i = 0; i < MAX_BISECTION_ITERATIONS && low < high; ++i) {
    double currentX = SampleCurve(p1x, p2x, t);
    if (std::fabs(currentX - input) < EPSILON) {
      return t;
    }
    if (input > currentX) {
      low = t;
    } else {
      high = t;
    }
    t = (low + high) * 0.5;
  }
  return t;
}

double SolveBezierEasing(double p1x, double p1y, double p2x, double p2y, double input) {
  if (input <= 0.0) {
    return 0.0;
  }
  if (input >= 1.0) {
    return 1.0;
  }
  // Linear shortcut: control points on the diagonal collapse cubic-bezier into the identity.
  if (p1x == p1y && p2x == p2y) {
    return input;
  }
  double t = SolveCurveX(p1x, p2x, input);
  return std::clamp(SampleCurve(p1y, p2y, t), 0.0, 1.0);
}

}  // namespace pagx
