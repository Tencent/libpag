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

#include "pagx/nodes/LayerFilter.h"

namespace pagx {

/**
 * A glow filter (spec 7.5): blooms bright areas of the layer with a threshold-controlled
 * blurred copy, migrated from the legacy Glow effect.
 */
class GlowFilter : public LayerFilter {
 public:
  /**
   * The luminance threshold above which pixels contribute to the glow, in 0-1 percent space.
   * The default value is 0.
   */
  float glowThreshold = 0.0f;

  /**
   * The glow blur radius in pixels. The default value is 0.
   */
  float glowRadius = 0.0f;

  /**
   * The glow strength multiplier. The default value is 1.
   */
  float glowIntensity = 1.0f;

  NodeType nodeType() const override {
    return NodeType::GlowFilter;
  }

 private:
  GlowFilter() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
