/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "MultiDimensionPoint3DKeyframe.h"

namespace pag {
MultiDimensionPoint3DKeyframe::~MultiDimensionPoint3DKeyframe() {
  delete xInterpolator;
  delete yInterpolator;
  delete zInterpolator;
}

void MultiDimensionPoint3DKeyframe::initialize() {
  if (interpolationType == KeyframeInterpolationType::Bezier) {
    xInterpolator = new BezierEasing(this->bezierOut[0], this->bezierIn[0]);
    yInterpolator = new BezierEasing(this->bezierOut[1], this->bezierIn[1]);
    zInterpolator = new BezierEasing(this->bezierOut[2], this->bezierIn[2]);
  } else {
    xInterpolator = new Interpolator();
    yInterpolator = new Interpolator();
    zInterpolator = new Interpolator();
  }
}

Point3D MultiDimensionPoint3DKeyframe::getValueAt(Frame time) {
  auto progress = static_cast<float>(time - this->startTime) / (this->endTime - this->startTime);
  auto xProgress = xInterpolator->getInterpolation(progress);
  auto yProgress = yInterpolator->getInterpolation(progress);
  auto zProgress = zInterpolator->getInterpolation(progress);
  auto x = Interpolate(this->startValue.x, this->endValue.x, xProgress);
  auto y = Interpolate(this->startValue.y, this->endValue.y, yProgress);
  auto z = Interpolate(this->startValue.z, this->endValue.z, zProgress);
  return {x, y, z};
}

}  // namespace pag
