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

#include "SpatialPointKeyframe.h"

namespace pag {
void SpatialPointKeyframe::initialize() {
  SingleEaseKeyframe<Point>::initialize();
  auto point1 = Point::Make(startValue.x + spatialOut.x, startValue.y + spatialOut.y);
  auto point2 = Point::Make(endValue.x + spatialIn.x, endValue.y + spatialIn.y);
  spatialBezier = BezierPath::Build(startValue, point1, point2, endValue, 0.05f);
}

Point SpatialPointKeyframe::getValueAt(Frame time) {
  auto progress = getProgress(time);
  return spatialBezier->getPosition(progress);
}
}  // namespace pag
