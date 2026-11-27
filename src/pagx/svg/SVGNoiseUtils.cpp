///////////////////////////////////////////////////////////////////////////////////////////////
//
// Tencent is pleased to support the open source community by making libpag available.
//
// Copyright (C) 2026 Tencent. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
// except in compliance with the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the
// License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
// either express or implied. See the License for the specific language governing permissions
// and limitations under the License.
//
///////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/svg/SVGNoiseUtils.h"
#include <algorithm>
#include <cmath>

namespace pagx {

NoiseBand MakeDarkNoiseBand(float density) {
  auto d = std::clamp(density, 0.0f, 1.0f);
  NoiseBand band;
  band.lower = std::clamp(static_cast<int>(std::lround(-25.0f * d + 25.0f)), 0, 99);
  band.upper = std::clamp(static_cast<int>(std::lround(24.0f * d + 25.0f)), 0, 99);
  return band;
}

NoiseBand MakeBrightNoiseBand(float density) {
  auto d = std::clamp(density, 0.0f, 1.0f);
  NoiseBand band;
  band.lower = std::clamp(static_cast<int>(std::lround(-24.0f * d + 74.0f)), 0, 99);
  band.upper = std::clamp(static_cast<int>(std::lround(25.0f * d + 74.0f)), 0, 99);
  return band;
}

float NoiseCoverageToDensity(float coverage) {
  // The dark band spans (49 * density + 1) percentiles, so density = (coverage * 100 - 1) / 49.
  return std::clamp((coverage * 100.0f - 1.0f) / 49.0f, 0.0f, 1.0f);
}

}  // namespace pagx
