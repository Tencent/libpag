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
 * A conic (sweep) gradient color source that produces a gradient sweeping around a center point.
 * By default the gradient lives in each geometry's normalized (0, 0)-(1, 1) bounding box space
 * (see Gradient::fitsToGeometry).
 */
class ConicGradient : public Gradient {
 public:
  /**
   * The center point of the gradient. Defaults to (0.5, 0.5), the center of the geometry's
   * normalized bounding box when fitsToGeometry is true.
   */
  Point center = {0.5f, 0.5f};

  /**
   * The starting angle of the gradient sweep in degrees. The default value is 0.
   */
  float startAngle = 0.0f;

  /**
   * The ending angle of the gradient sweep in degrees. The default value is 360.
   */
  float endAngle = 360.0f;

  NodeType nodeType() const override {
    return NodeType::ConicGradient;
  }

 private:
  ConicGradient() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
