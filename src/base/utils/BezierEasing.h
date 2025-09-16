/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "BezierPath.h"
#include "BezierPath3D.h"
#include "Interpolator.h"

namespace pag {
class BezierEasing : public Interpolator {
 public:
  BezierEasing(const Point& control1, const Point& control2);

  /**
   * Maps a value representing the elapsed fraction of an animation to a value that represents the
   * interpolated fraction. This interpolated value is then multiplied by the change in value of an
   * animation to derive the animated value at the current elapsed animation time.
   * @param input input A value between 0 and 1.0 indicating our current point in the animation
   * where 0 represents the start and 1.0 represents the end.
   * @return The interpolation value. This value can be more than 1.0 for interpolators which
   * overshoot their targets, or less than 0 for interpolators that undershoot their targets.
   */
  float getInterpolation(float input) override;

 private:
  std::shared_ptr<BezierPath> bezierPath = nullptr;
};
}  // namespace pag
