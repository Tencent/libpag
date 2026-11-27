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

#pragma once

namespace pagx {

struct NoiseBand {
  int lower = 0;
  int upper = 0;
};

// Maps a PAGX noise density to the luminance percentile band used by the SVG export. The dark
// and bright bands share the same geometry so a density survives an SVG export/import round trip.
NoiseBand MakeDarkNoiseBand(float density);
NoiseBand MakeBrightNoiseBand(float density);

// Inverse of the dark-band geometry: the density whose band covers the given fraction of the
// luminance range. Used when importing SVG alpha tables written by the exporter or by tools that
// threshold fractal noise the same way.
float NoiseCoverageToDensity(float coverage);

}  // namespace pagx
