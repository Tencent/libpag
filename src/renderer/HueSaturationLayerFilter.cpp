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

#include "HueSaturationLayerFilter.h"
#include <utility>
#include "rendering/filters/HueSaturationFilter.h"
#include "tgfx/core/ImageFilter.h"
#include "tgfx/core/Point.h"

namespace pagx {

std::shared_ptr<HueSaturationLayerFilter> HueSaturationLayerFilter::Make(
    float hue, float saturation, float lightness, bool colorize, float colorizeHue,
    float colorizeSaturation, float colorizeLightness) {
  auto filter = std::shared_ptr<HueSaturationLayerFilter>(new HueSaturationLayerFilter());
  filter->_hue = hue;
  filter->_saturation = saturation;
  filter->_lightness = lightness;
  filter->_colorize = colorize;
  filter->_colorizeHue = colorizeHue;
  filter->_colorizeSaturation = colorizeSaturation;
  filter->_colorizeLightness = colorizeLightness;
  return filter;
}

void HueSaturationLayerFilter::setHue(float value) {
  if (_hue == value) {
    return;
  }
  _hue = value;
  invalidateFilter();
}

void HueSaturationLayerFilter::setSaturation(float value) {
  if (_saturation == value) {
    return;
  }
  _saturation = value;
  invalidateFilter();
}

void HueSaturationLayerFilter::setLightness(float value) {
  if (_lightness == value) {
    return;
  }
  _lightness = value;
  invalidateFilter();
}

void HueSaturationLayerFilter::setColorize(bool value) {
  if (_colorize == value) {
    return;
  }
  _colorize = value;
  invalidateFilter();
}

void HueSaturationLayerFilter::setColorizeHue(float value) {
  if (_colorizeHue == value) {
    return;
  }
  _colorizeHue = value;
  invalidateFilter();
}

void HueSaturationLayerFilter::setColorizeSaturation(float value) {
  if (_colorizeSaturation == value) {
    return;
  }
  _colorizeSaturation = value;
  invalidateFilter();
}

void HueSaturationLayerFilter::setColorizeLightness(float value) {
  if (_colorizeLightness == value) {
    return;
  }
  _colorizeLightness = value;
  invalidateFilter();
}

std::shared_ptr<tgfx::Image> HueSaturationLayerFilter::onFilterImage(
    std::shared_ptr<tgfx::Image> input, float, const tgfx::Rect&, tgfx::Point* offset,
    const tgfx::Point&) {
  if (input == nullptr) {
    return nullptr;
  }
  // Mirrors the legacy HueSaturationFilter::Apply: the values carry over raw and the
  // RuntimeFilter normalizes (hue / 360, saturation and lightness / 100) in its uniforms.
  auto filter = std::make_shared<pag::HueSaturationFilter>(nullptr, _hue, _saturation, _lightness,
                                                           _colorize ? 1.0f : 0.0f, _colorizeHue,
                                                           _colorizeSaturation, _colorizeLightness);
  return input->makeWithFilter(tgfx::ImageFilter::Runtime(std::move(filter)), offset);
}

}  // namespace pagx
