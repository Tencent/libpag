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

#include <vector>
#include "pagx/nodes/ColorSource.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/types/Matrix.h"

namespace pagx {

/**
 * Base class for all gradient color sources. Gradient defines the attributes shared by linear,
 * radial, conic, and diamond gradients: a transformation matrix, a sequence of ColorStops, and
 * the coordinate-space selector fitsToGeometry.
 *
 * When fitsToGeometry is true (the default), the gradient's geometric parameters (start/end
 * points, center, radius, etc.) are interpreted in a normalized 0-1 coordinate space mapped to
 * each geometry's bounding box. When fitsToGeometry is false, the parameters are interpreted in
 * the geometry's local coordinate space.
 */
class Gradient : public ColorSource {
 public:
  /**
   * The transformation matrix applied to the gradient. The matrix operates on the gradient's
   * local coordinate space (the space where startPoint/endPoint/center/radius are expressed).
   */
  Matrix matrix = {};

  /**
   * The color stops defining the gradient colors and positions.
   */
  std::vector<ColorStop*> colorStops = {};

  /**
   * Whether the gradient parameters are interpreted relative to each geometry's bounding box.
   * When true (the default), the parameters live in a (0, 0)-(1, 1) coordinate space mapped to
   * each geometry's bounding box. When false, the parameters are in the geometry's local
   * coordinate space.
   */
  bool fitsToGeometry = true;

 protected:
  Gradient() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
