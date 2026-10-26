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
 * The direction of a radial blur (spec 7.5), matching the legacy effect.
 */
enum class RadialBlurMode : uint8_t {
  Spin = 0,
  Zoom = 1,
};

/**
 * The sampling quality of a radial blur (spec 7.5).
 */
enum class RadialBlurAntialias : uint8_t {
  Low = 0,
  High = 1,
};

/**
 * A radial blur filter (spec 7.5): spins or zooms the layer content around a center,
 * migrated from the legacy RadialBlur effect.
 */
class RadialBlurFilter : public LayerFilter {
 public:
  /**
   * The blur strength. The default value is 0.
   */
  float amount = 0.0f;

  /**
   * The blur center in layer coordinates. The default value is (0, 0).
   */
  Point center = {};

  RadialBlurMode mode = RadialBlurMode::Spin;

  RadialBlurAntialias antialias = RadialBlurAntialias::Low;

  NodeType nodeType() const override {
    return NodeType::RadialBlurFilter;
  }

 private:
  RadialBlurFilter() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
