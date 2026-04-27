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
 * A linear gradient color source that produces a gradient along a line between two points. By
 * default the start and end points are interpreted in each geometry's normalized (0, 0)-(1, 1)
 * bounding box space (see Gradient::fitsToGeometry).
 */
class LinearGradient : public Gradient {
 public:
  /**
   * The starting point of the gradient line. Defaults to (0, 0), the top-left corner of the
   * geometry's normalized bounding box when fitsToGeometry is true.
   */
  Point startPoint = {0.0f, 0.0f};

  /**
   * The ending point of the gradient line. Defaults to (1, 1), the bottom-right corner of the
   * geometry's normalized bounding box when fitsToGeometry is true.
   */
  Point endPoint = {1.0f, 1.0f};

  NodeType nodeType() const override {
    return NodeType::LinearGradient;
  }

 private:
  LinearGradient() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
