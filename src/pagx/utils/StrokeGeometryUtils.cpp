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

#include "pagx/utils/StrokeGeometryUtils.h"
#include <algorithm>
#include "pagx/nodes/Stroke.h"

namespace pagx {

float StrokeAlignInset(const Stroke* stroke) {
  if (stroke == nullptr || stroke->width <= 0) {
    return 0.0f;
  }
  switch (stroke->align) {
    case StrokeAlign::Inside:
      return stroke->width / 2.0f;
    case StrokeAlign::Outside:
      return -stroke->width / 2.0f;
    case StrokeAlign::Center:
    default:
      return 0.0f;
  }
}

void ApplyStrokeBoxInset(const Stroke* stroke, float& x, float& y, float& w, float& h,
                         float* roundness) {
  float inset = StrokeAlignInset(stroke);
  if (inset == 0.0f) {
    return;
  }
  float maxInset = std::min(w, h) / 2.0f;
  if (inset > maxInset) {
    inset = maxInset;
  }
  x += inset;
  y += inset;
  w -= inset * 2.0f;
  h -= inset * 2.0f;
  if (roundness) {
    *roundness = std::max(0.0f, *roundness - inset);
  }
}

}  // namespace pagx
