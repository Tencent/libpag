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

#include "MultiDimensionPointKeyframe.h"

namespace pag {
MultiDimensionPointKeyframe::~MultiDimensionPointKeyframe() {
  delete xInterpolator;
  delete yInterpolator;
}

void MultiDimensionPointKeyframe::initialize() {
  if (interpolationType == KeyframeInterpolationType::Bezier) {
    xInterpolator = new BezierEasing(this->bezierOut[0], this->bezierIn[0]);
    yInterpolator = new BezierEasing(this->bezierOut[1], this->bezierIn[1]);
  } else {
    xInterpolator = new Interpolator();
    yInterpolator = new Interpolator();
  }
}

Point MultiDimensionPointKeyframe::getValueAt(Frame time) {
  auto progress = static_cast<float>(time - this->startTime) / (this->endTime - this->startTime);
  auto xProgress = xInterpolator->getInterpolation(progress);
  auto yProgress = yInterpolator->getInterpolation(progress);
  auto x = Interpolate(this->startValue.x, this->endValue.x, xProgress);
  auto y = Interpolate(this->startValue.y, this->endValue.y, yProgress);
  return {x, y};
}

}  // namespace pag
