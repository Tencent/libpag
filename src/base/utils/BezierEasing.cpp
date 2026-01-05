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

#include "BezierEasing.h"

namespace pag {
BezierEasing::BezierEasing(const Point& control1, const Point& control2) {
  bezierPath = BezierPath::Build(Point::Zero(), control1, control2, Point::Make(1, 1), 0.005f);
}

float BezierEasing::getInterpolation(float input) {
  if (input <= 0) {
    return 0;
  }
  if (input >= 1) {
    return 1;
  }
  return bezierPath->getY(input);
}
}  // namespace pag
