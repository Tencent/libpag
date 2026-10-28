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

#include "CornerPinLayerFilter.h"
#include <utility>
#include "rendering/filters/CornerPinFilter.h"
#include "tgfx/core/ImageFilter.h"

namespace pagx {

std::shared_ptr<CornerPinLayerFilter> CornerPinLayerFilter::Make(Point upperLeft, Point upperRight,
                                                                 Point lowerLeft,
                                                                 Point lowerRight) {
  auto filter = std::shared_ptr<CornerPinLayerFilter>(new CornerPinLayerFilter());
  filter->_upperLeft = upperLeft;
  filter->_upperRight = upperRight;
  filter->_lowerLeft = lowerLeft;
  filter->_lowerRight = lowerRight;
  return filter;
}

void CornerPinLayerFilter::setCorner(int index, float x, float y) {
  auto* corner = index == 0   ? &_upperLeft
                 : index == 1 ? &_upperRight
                 : index == 2 ? &_lowerLeft
                              : &_lowerRight;
  if (corner->x == x && corner->y == y) {
    return;
  }
  *corner = {x, y};
  invalidateFilter();
}

std::shared_ptr<tgfx::ImageFilter> CornerPinLayerFilter::onCreateImageFilter(float scale) {
  // Mirrors the legacy CornerPinFilter::Apply: the layer-space corner points scale into
  // input-texture pixels, ordered [lowerLeft, lowerRight, upperLeft, upperRight] as the legacy
  // RuntimeFilter expects.
  pag::Point points[4] = {
      {_lowerLeft.x * scale, _lowerLeft.y * scale},
      {_lowerRight.x * scale, _lowerRight.y * scale},
      {_upperLeft.x * scale, _upperLeft.y * scale},
      {_upperRight.x * scale, _upperRight.y * scale},
  };
  auto filter = std::make_shared<pag::CornerPinFilter>(nullptr, points);
  return tgfx::ImageFilter::Runtime(std::move(filter));
}

}  // namespace pagx
