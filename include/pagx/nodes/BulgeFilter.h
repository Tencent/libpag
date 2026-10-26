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
#include "pagx/types/Point.h"

namespace pagx {

/**
 * A bulge filter (spec 7.5): radially distorts the layer around a center with elliptical
 * radii, migrated from the legacy Bulge effect.
 */
class BulgeFilter : public LayerFilter {
 public:
  /**
   * The horizontal radius of the bulge ellipse in pixels. The default value is 0.
   */
  float horizontalRadius = 0.0f;

  /**
   * The vertical radius of the bulge ellipse in pixels. The default value is 0.
   */
  float verticalRadius = 0.0f;

  /**
   * The center of the bulge in layer coordinates. The default value is (0, 0).
   */
  Point bulgeCenter = {};

  /**
   * The bulge height; positive values push outward, negative values pinch inward.
   * The default value is 0.
   */
  float bulgeHeight = 0.0f;

  /**
   * The taper radius controlling how the effect falls off toward the ellipse edge.
   * The default value is 0.
   */
  float taperRadius = 0.0f;

  /**
   * Whether the effect pins the surrounding pixels in place. The default value is false.
   */
  bool pinning = false;

  NodeType nodeType() const override {
    return NodeType::BulgeFilter;
  }

 private:
  BulgeFilter() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
