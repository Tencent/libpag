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
//  license is distributed on an "AS IS" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include "pagx/types/Point.h"
#include "tgfx/layers/filters/LayerImageFilter.h"

namespace pagx {

// Layer filter for the PAGX CornerPinFilter node (spec 7.5). Wraps the legacy perspective quad
// warp RuntimeFilter (src/rendering/filters/CornerPinFilter.cpp) through a tgfx::LayerImageFilter
// so the filter bounds delegate to the RuntimeFilter's quad bounding box - matching the legacy
// CornerPinEffect::transformBounds - and runs cache-less because the PAGX layer path has no
// pag::RenderCache. Corner points live in layer coordinates and scale by the content scale into
// input-texture pixels, exactly like the legacy sourceScale multiplication.
class CornerPinLayerFilter : public tgfx::LayerImageFilter {
 public:
  static std::shared_ptr<CornerPinLayerFilter> Make(Point upperLeft, Point upperRight,
                                                    Point lowerLeft, Point lowerRight);

  float upperLeftX() const {
    return _upperLeft.x;
  }

  float upperLeftY() const {
    return _upperLeft.y;
  }

  float upperRightX() const {
    return _upperRight.x;
  }

  float upperRightY() const {
    return _upperRight.y;
  }

  float lowerLeftX() const {
    return _lowerLeft.x;
  }

  float lowerLeftY() const {
    return _lowerLeft.y;
  }

  float lowerRightX() const {
    return _lowerRight.x;
  }

  float lowerRightY() const {
    return _lowerRight.y;
  }

  void setCorner(int index, float x, float y);

 protected:
  std::shared_ptr<tgfx::ImageFilter> onCreateImageFilter(float scale) override;

 private:
  Point _upperLeft = {};
  Point _upperRight = {};
  Point _lowerLeft = {};
  Point _lowerRight = {};
};

}  // namespace pagx
