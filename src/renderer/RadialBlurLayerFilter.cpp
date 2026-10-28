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

#include "RadialBlurLayerFilter.h"
#include <utility>
#include "rendering/filters/RadialBlurFilter.h"
#include "tgfx/core/ImageFilter.h"
#include "tgfx/core/Point.h"

namespace pagx {

std::shared_ptr<RadialBlurLayerFilter> RadialBlurLayerFilter::Make(float amount, Point center) {
  auto filter = std::shared_ptr<RadialBlurLayerFilter>(new RadialBlurLayerFilter());
  filter->_amount = amount;
  filter->_center = center;
  return filter;
}

void RadialBlurLayerFilter::setAmount(float value) {
  if (_amount == value) {
    return;
  }
  _amount = value;
  invalidateFilter();
}

void RadialBlurLayerFilter::setCenter(float x, float y) {
  if (_center.x == x && _center.y == y) {
    return;
  }
  _center = {x, y};
  invalidateFilter();
}

std::shared_ptr<tgfx::Image> RadialBlurLayerFilter::onFilterImage(
    std::shared_ptr<tgfx::Image> input, float, const tgfx::Rect& contentBounds,
    tgfx::Point* offset) {
  if (input == nullptr) {
    return nullptr;
  }
  printf("[rb-render] input=%dx%d contentBounds=(%f,%f,%f,%f) amount=%f center=(%f,%f)\n",
         input->width(), input->height(), contentBounds.left, contentBounds.top,
         contentBounds.right, contentBounds.bottom, _amount, _center.x, _center.y);
  // Mirrors the legacy RadialBlurFilter::Apply math: the amount scales by 1/160 and clamps at
  // 0.25, and the center normalizes against the content bounds size into uv space.
  auto amount = static_cast<double>(_amount) * 0.00625;
  if (amount > 0.25) {
    amount = 0.25;
  }
  auto width = contentBounds.width();
  auto height = contentBounds.height();
  tgfx::Point center = {_center.x / width, _center.y / height};
  auto filter = std::make_shared<pag::RadialBlurFilter>(nullptr, amount, center);
  return input->makeWithFilter(tgfx::ImageFilter::Runtime(std::move(filter)), offset);
}

}  // namespace pagx
