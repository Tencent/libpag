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
//  license is distributed on an "AS IS" basis, without WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "pagx/nodes/LayerFilter.h"

namespace pagx {

/**
 * A hue/saturation filter (spec 7.5): rotates hue, scales saturation and offsets lightness in
 * HSV space, or replaces the color with a fixed colorize triplet, migrated from the legacy
 * HueSaturation effect. The legacy stores per-channel-control slots but its renderer only ever
 * consumes the selected slot, so the node flattens to the selected values.
 */
class HueSaturationFilter : public LayerFilter {
 public:
  /**
   * The hue rotation in degrees. The default value is 0.
   */
  float hue = 0.0f;

  /**
   * The saturation scale in percent (0 keeps the saturation, -100 desaturates). The default
   * value is 0.
   */
  float saturation = 0.0f;

  /**
   * The lightness offset in percent. The default value is 0.
   */
  float lightness = 0.0f;

  /**
   * Whether the colorize triplet replaces the source color instead of adjusting it. The default
   * value is false.
   */
  bool colorize = false;

  /**
   * The colorize hue in degrees. The default value is 0.
   */
  float colorizeHue = 0.0f;

  /**
   * The colorize saturation in percent. The default value is 0.
   */
  float colorizeSaturation = 0.0f;

  /**
   * The colorize lightness in percent. The default value is 0.
   */
  float colorizeLightness = 0.0f;

  NodeType nodeType() const override {
    return NodeType::HueSaturationFilter;
  }

 private:
  HueSaturationFilter() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
