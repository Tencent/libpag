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

#include "base/utils/Interpolate.h"
#include "pag/file.h"

namespace pag {
void GradientColor::interpolate(const GradientColor& other, GradientColor* result, float t) {
  result->alphaStops = alphaStops;
  result->colorStops = colorStops;
  auto alphaCount = std::min(alphaStops.size(), other.alphaStops.size());
  for (size_t i = 0; i < alphaCount; i++) {
    auto start = alphaStops[i].opacity;
    auto end = other.alphaStops[i].opacity;
    result->alphaStops[i].opacity = Interpolate(start, end, t);
  }
  auto colorCount = std::min(colorStops.size(), other.colorStops.size());
  for (size_t i = 0; i < colorCount; i++) {
    auto start = colorStops[i].color;
    auto end = other.colorStops[i].color;
    result->colorStops[i].color = Interpolate(start, end, t);
  }
}
}  // namespace pag
