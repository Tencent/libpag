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

#include "BulgeLayerFilter.h"
#include <utility>
#include "rendering/filters/BulgeFilter.h"
#include "tgfx/core/ImageFilter.h"
#include "tgfx/core/MapDirection.h"
#include "tgfx/core/Point.h"

namespace pagx {

std::shared_ptr<BulgeLayerFilter> BulgeLayerFilter::Make(float horizontalRadius,
                                                         float verticalRadius, Point bulgeCenter,
                                                         float bulgeHeight, bool pinning) {
  auto filter = std::shared_ptr<BulgeLayerFilter>(new BulgeLayerFilter());
  filter->_horizontalRadius = horizontalRadius;
  filter->_verticalRadius = verticalRadius;
  filter->_bulgeCenter = bulgeCenter;
  filter->_bulgeHeight = bulgeHeight;
  filter->_pinning = pinning;
  return filter;
}

void BulgeLayerFilter::setHorizontalRadius(float value) {
  if (_horizontalRadius == value) {
    return;
  }
  _horizontalRadius = value;
  invalidateFilter();
}

void BulgeLayerFilter::setVerticalRadius(float value) {
  if (_verticalRadius == value) {
    return;
  }
  _verticalRadius = value;
  invalidateFilter();
}

void BulgeLayerFilter::setBulgeCenter(float x, float y) {
  if (_bulgeCenter.x == x && _bulgeCenter.y == y) {
    return;
  }
  _bulgeCenter = {x, y};
  invalidateFilter();
}

void BulgeLayerFilter::setBulgeHeight(float value) {
  if (_bulgeHeight == value) {
    return;
  }
  _bulgeHeight = value;
  invalidateFilter();
}

void BulgeLayerFilter::setPinning(bool value) {
  if (_pinning == value) {
    return;
  }
  _pinning = value;
  invalidateFilter();
}

std::shared_ptr<tgfx::Image> BulgeLayerFilter::onFilterImage(std::shared_ptr<tgfx::Image> input,
                                                             float, const tgfx::Rect& contentBounds,
                                                             tgfx::Point* offset,
                                                             const tgfx::Point&) {
  if (input == nullptr || contentBounds.isEmpty()) {
    return input;
  }
  // Mirrors the legacy BulgeFilter::Apply normalization: the radii and center convert into uv
  // fractions of the content bounds. The tgfx content bounds carry a zero origin, which matches
  // the legacy content bounds whenever the layer content starts at the layer-local origin.
  auto filter = std::make_shared<pag::BulgeFilter>(
      nullptr, _horizontalRadius / contentBounds.width(), _verticalRadius / contentBounds.height(),
      pag::Point{_bulgeCenter.x / contentBounds.width(), _bulgeCenter.y / contentBounds.height()},
      _bulgeHeight, _pinning ? 1.0f : 0.0f);
  return input->makeWithFilter(tgfx::ImageFilter::Runtime(std::move(filter)), offset);
}

tgfx::Rect BulgeLayerFilter::filterBounds(const tgfx::Rect& srcRect, float contentScale,
                                          tgfx::MapDirection direction) {
  if (srcRect.isEmpty()) {
    return srcRect;
  }
  // The reverse pass asks how much input fills a destination rect: the bulge samples inside
  // its own region, so the input needed is the destination itself (the legacy never needed a
  // reverse mapping - its input texture was fixed at the content bounds).
  if (direction == tgfx::MapDirection::Reverse) {
    return srcRect;
  }
  // The legacy RuntimeFilter bounds expansion is self-contained given a rect and uv params, so
  // the layer-level mapping normalizes against the incoming rect (layer-local at scale 1.0 for
  // the forward pass, image space at the content scale for the reverse pass - both produce the
  // same uv fractions).
  std::shared_ptr<pag::RuntimeFilter> filter = std::make_shared<pag::BulgeFilter>(
      nullptr, _horizontalRadius * contentScale / srcRect.width(),
      _verticalRadius * contentScale / srcRect.height(),
      pag::Point{(_bulgeCenter.x * contentScale - srcRect.left) / srcRect.width(),
                 (_bulgeCenter.y * contentScale - srcRect.top) / srcRect.height()},
      _bulgeHeight, _pinning ? 1.0f : 0.0f);
  return filter->filterBounds(srcRect, tgfx::MapDirection::Forward);
}

}  // namespace pagx
