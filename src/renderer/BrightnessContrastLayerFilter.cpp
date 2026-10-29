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
//  license is distributed on an "AS IS" basis, without warranties OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "BrightnessContrastLayerFilter.h"
#include <utility>
#include "rendering/filters/BrightnessContrastFilter.h"
#include "tgfx/core/ImageFilter.h"
#include "tgfx/core/Point.h"

namespace pagx {

std::shared_ptr<BrightnessContrastLayerFilter> BrightnessContrastLayerFilter::Make(float brightness,
                                                                                   float contrast) {
  auto filter = std::shared_ptr<BrightnessContrastLayerFilter>(new BrightnessContrastLayerFilter());
  filter->_brightness = brightness;
  filter->_contrast = contrast;
  return filter;
}

void BrightnessContrastLayerFilter::setBrightness(float value) {
  if (_brightness == value) {
    return;
  }
  _brightness = value;
  invalidateFilter();
}

void BrightnessContrastLayerFilter::setContrast(float value) {
  if (_contrast == value) {
    return;
  }
  _contrast = value;
  invalidateFilter();
}

std::shared_ptr<tgfx::Image> BrightnessContrastLayerFilter::onFilterImage(
    std::shared_ptr<tgfx::Image> input, float, const tgfx::Rect&, tgfx::Point* offset,
    const tgfx::Point&) {
  if (input == nullptr) {
    return nullptr;
  }
  // Mirrors the legacy BrightnessContrastFilter::Apply: the values carry over raw and the
  // RuntimeFilter normalizes (brightness / 250 or / 650 by sign, contrast 1 + / 300) in its
  // uniforms.
  auto filter = std::make_shared<pag::BrightnessContrastFilter>(nullptr, _brightness, _contrast);
  return input->makeWithFilter(tgfx::ImageFilter::Runtime(std::move(filter)), offset);
}

}  // namespace pagx
