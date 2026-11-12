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
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "pagx/types/Point.h"
#include "tgfx/core/Path.h"

namespace pagx {

class Polystar;

/**
 * Field snapshot of a Polystar resolved into the parent coordinate space. The static bake in
 * ModifierResolver and the exporter's per-frame `d` sampling share this struct so both produce
 * identical geometry for the same authored values.
 */
struct PolystarSample {
  bool polygon = false;
  float pointCount = 0;
  float outerRadius = 0;
  float innerRadius = 0;
  float rotation = 0;
  float outerRoundness = 0;
  float innerRoundness = 0;
  bool reversed = false;
  Point center = {};
  float scale = 1.0f;
  // When true, every segment is emitted as a cubic even at zero roundness. A `d` track needs one
  // stable verb list across frames, and a zero-roundness cubic collapses onto the straight line.
  bool forceCurves = false;
};

/**
 * Captures the node's fields together with its layout-resolved position and scale.
 */
PolystarSample SamplePolystar(const Polystar* star);

/**
 * Builds the star/polygon outline in parent space. Mirrors tgfx/src/layers/vectors/Polystar.cpp
 * so the exporter, the resolver and the renderer agree on fractional point counts, roundness and
 * reversed direction.
 */
tgfx::Path PolystarToTGFXPath(const PolystarSample& sample);

}  // namespace pagx
