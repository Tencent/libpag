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

#include <algorithm>
#include <cmath>
#include "pagx/runtime/BezierEasing.h"
#include "pagx/runtime/KeyframeEvaluator.h"

namespace pagx {

namespace {

// normalized-v1 models the segment as the step response of a second-order system whose duration
// is one normalized time unit. The natural frequency is derived from the damping ratio so that the
// response is at rest at the end of the segment: for critical damping the displacement residual is
// (1 + omega) * e^(-omega), and the decay below is the omega at which that residual reaches 1% of
// the span. For zeta < 1 both the displacement and velocity envelopes decay as e^(-zeta * omega),
// so the same constant bounds both; for zeta > 1 the slow mode decays as
// e^(-(zeta - sqrt(zeta^2 - 1)) * omega) and the frequency is scaled by the inverse exponent.
constexpr double SPRING_SETTLE_DECAY = 6.638;

double ClampBounce(float bounce) {
  if (!std::isfinite(bounce)) {
    return 0.0;
  }
  return std::clamp(static_cast<double>(bounce), static_cast<double>(MinSpringBounce),
                    static_cast<double>(MaxSpringBounce));
}

double SpringNaturalFrequency(double zeta) {
  if (zeta < 1.0) {
    return SPRING_SETTLE_DECAY / zeta;
  }
  if (zeta > 1.0) {
    // Equivalent to C / (zeta - sqrt(zeta^2 - 1)), written as the sum to avoid cancellation.
    return SPRING_SETTLE_DECAY * (zeta + std::sqrt(zeta * zeta - 1.0));
  }
  return SPRING_SETTLE_DECAY;
}

// Unit step response. time is the normalized segment position in [0, 1].
double SpringStepResponse(double zeta, double omega, double time) {
  if (time <= 0.0) {
    return 0.0;
  }
  double decay = std::exp(-zeta * omega * time);
  if (zeta < 1.0) {
    double damped = std::sqrt(1.0 - zeta * zeta);
    double angle = omega * damped * time;
    return 1.0 - decay * (std::cos(angle) + zeta / damped * std::sin(angle));
  }
  if (zeta > 1.0) {
    double overdamped = std::sqrt(zeta * zeta - 1.0);
    double angle = omega * overdamped * time;
    return 1.0 - decay * (std::cosh(angle) + zeta / overdamped * std::sinh(angle));
  }
  return 1.0 - decay * (1.0 + omega * time);
}

double SolveSpringEasing(const SpringEasing& spring, double progress) {
  if (progress <= 0.0) {
    return 0.0;
  }
  if (progress >= 1.0) {
    return 1.0;
  }
  double zeta = 1.0 - ClampBounce(spring.bounce);
  double omega = SpringNaturalFrequency(zeta);
  // Endpoint normalization pins e(0) = 0 and e(1) = 1 exactly while preserving the overshoot of
  // the raw response between them.
  double end = SpringStepResponse(zeta, omega, 1.0);
  if (!std::isfinite(end) || std::fabs(end) < 1.0e-6) {
    return progress;
  }
  return SpringStepResponse(zeta, omega, progress) / end;
}

}  // namespace

double EvaluateKeyframeInterpolationProgress(KeyframeInterpolationType interpolation,
                                             const Point& bezierOut, const Point& bezierIn,
                                             const SpringEasing& spring, double progress) {
  switch (interpolation) {
    case KeyframeInterpolationType::None:
    case KeyframeInterpolationType::Hold:
      return 0.0;
    case KeyframeInterpolationType::Linear:
      return progress;
    case KeyframeInterpolationType::Bezier:
      return SolveBezierEasing(bezierOut.x, bezierOut.y, bezierIn.x, bezierIn.y, progress);
    case KeyframeInterpolationType::Spring:
      return SolveSpringEasing(spring, progress);
  }
  return progress;
}

}  // namespace pagx
