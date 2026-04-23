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

#pragma once

#include "pagx/nodes/Gradient.h"
#include "pagx/types/Point.h"

namespace pagx {

/**
 * A radial gradient color source that produces a gradient radiating from a center point. By
 * default the center and radius are interpreted in the geometry's normalized 0-1 bounding box
 * space (see Gradient::fitsToGeometry).
 */
class RadialGradient : public Gradient {
 public:
  /**
   * The center point of the gradient. Defaults to (0.5, 0.5), the center of the geometry's
   * normalized bounding box when fitsToGeometry is true.
   */
  Point center = {0.5f, 0.5f};

  /**
   * The radius of the gradient circle. Defaults to 0.5, reaching the bounding-box edge along each
   * axis when fitsToGeometry is true.
   */
  float radius = 0.5f;

  NodeType nodeType() const override {
    return NodeType::RadialGradient;
  }

 private:
  RadialGradient() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
