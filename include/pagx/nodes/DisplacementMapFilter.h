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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LayerFilter.h"

namespace pagx {

/**
 * The source channel of a displacement map sample (spec 7.5), matching the legacy effect.
 */
enum class DisplacementMapSource : uint8_t {
  Red = 0,
  Green = 1,
  Blue = 2,
  Alpha = 3,
  Luminance = 4,
  Hue = 5,
  Lightness = 6,
  Saturation = 7,
  Full = 8,
  Half = 9,
  Off = 10,
};

/**
 * How the displacement map layer is fitted to the effect area (spec 7.5).
 */
enum class DisplacementMapBehavior : uint8_t {
  CenterMap = 0,
  StretchMapToFit = 1,
  TileMap = 2,
};

class Layer;

/**
 * A displacement map filter (spec 7.5): shifts pixels with a map layer's channel values,
 * migrated from the legacy DisplacementMap effect. The map layer is referenced by node index
 * in the binary and by @id in XML, the same linkage model as Layer::mask.
 */
class DisplacementMapFilter : public LayerFilter {
 public:
  DisplacementMapSource useForHorizontalDisplacement = DisplacementMapSource::Red;

  /**
   * The maximum horizontal displacement in pixels. The default value is 0.
   */
  float maxHorizontalDisplacement = 0.0f;

  DisplacementMapSource useForVerticalDisplacement = DisplacementMapSource::Red;

  /**
   * The maximum vertical displacement in pixels. The default value is 0.
   */
  float maxVerticalDisplacement = 0.0f;

  DisplacementMapBehavior displacementMapBehavior = DisplacementMapBehavior::CenterMap;

  /**
   * Whether pixels shifted outside the map area wrap around. The default value is false.
   */
  bool edgeBehavior = false;

  /**
   * Whether the output grows to fit displaced pixels. The default value is true.
   */
  bool expandOutput = true;

  /**
   * The layer whose pixels drive the displacement. Null disables the filter.
   */
  Layer* displacementMapLayer = nullptr;

  NodeType nodeType() const override {
    return NodeType::DisplacementMapFilter;
  }

 private:
  DisplacementMapFilter() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
