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
 */
class Gradient : public ColorSource {
 public:
  /**
   * The transformation matrix applied to the gradient, composed in the selected coordinate
   * space (see fitsToGeometry).
   */
  Matrix matrix = {};

  /**
   * The color stops defining the gradient colors and positions.
   */
  std::vector<ColorStop*> colorStops = {};

  /**
   * Selects the coordinate space for the gradient. When true (the default), the gradient lives
   * in a normalized (0, 0)-(1, 1) space mapped to each geometry's bounding box, and the fill
   * auto-fits per geometry. When false, the gradient lives in the parent container's (Layer or
   * Group) coordinate space (origin at (0, 0)), and multiple geometries in that container share
   * one continuous fill.
   */
  bool fitsToGeometry = true;

 protected:
  Gradient() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
