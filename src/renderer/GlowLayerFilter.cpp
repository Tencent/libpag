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

#include "GlowLayerFilter.h"
#include <cmath>
#include <utility>
#include "rendering/filters/glow/GlowBlurFilter.h"
#include "rendering/filters/glow/GlowMergeFilter.h"
#include "rendering/filters/utils/BlurTypes.h"
#include "tgfx/core/ImageFilter.h"
#include "tgfx/core/Point.h"

namespace pagx {

std::shared_ptr<GlowLayerFilter> GlowLayerFilter::Make(float glowThreshold, float glowRadius,
                                                       float glowIntensity) {
  auto filter = std::shared_ptr<GlowLayerFilter>(new GlowLayerFilter());
  filter->_glowThreshold = glowThreshold;
  filter->_glowRadius = glowRadius;
  filter->_glowIntensity = glowIntensity;
  return filter;
}

void GlowLayerFilter::setGlowThreshold(float value) {
  if (_glowThreshold == value) {
    return;
  }
  _glowThreshold = value;
  invalidateFilter();
}

void GlowLayerFilter::setGlowRadius(float value) {
  if (_glowRadius == value) {
    return;
  }
  _glowRadius = value;
  invalidateFilter();
}

void GlowLayerFilter::setGlowIntensity(float value) {
  if (_glowIntensity == value) {
    return;
  }
  _glowIntensity = value;
  invalidateFilter();
}

std::shared_ptr<tgfx::Image> GlowLayerFilter::onFilterImage(std::shared_ptr<tgfx::Image> input,
                                                            float, const tgfx::Rect&,
                                                            tgfx::Point* offset) {
  if (input == nullptr) {
    return nullptr;
  }
  // Mirrors the legacy GlowFilter::Apply math (src/rendering/filters/glow/GlowFilter.cpp): the
  // blur runs on a copy shrunk by (1 - radius / 1500) and samples with a per-axis step of
  // 1 / blurredSize, so the effect is resolution-adaptive rather than pixel-scaled.
  auto resizeRatio = 1.0f - _glowRadius / 1500.0f;
  auto blurWidth = ceilf(static_cast<float>(input->width()) * resizeRatio);
  auto blurHeight = ceilf(static_cast<float>(input->height()) * resizeRatio);
  if (blurWidth <= 0.0f || blurHeight <= 0.0f) {
    return input;
  }
  auto blurFilterH = std::make_shared<pag::GlowBlurRuntimeFilter>(
      nullptr, pag::BlurDirection::Horizontal, 1.0f / blurWidth, resizeRatio);
  auto imageFilterH = tgfx::ImageFilter::Runtime(std::move(blurFilterH));
  auto blurFilterV = std::make_shared<pag::GlowBlurRuntimeFilter>(
      nullptr, pag::BlurDirection::Vertical, 1.0f / blurHeight, 1.0f);
  auto imageFilterV = tgfx::ImageFilter::Runtime(std::move(blurFilterV));
  auto blurFilter = tgfx::ImageFilter::Compose(std::move(imageFilterH), std::move(imageFilterV));
  auto blurImage = input->makeWithFilter(std::move(blurFilter));
  if (blurImage == nullptr) {
    return input;
  }
  auto mergeFilter = tgfx::ImageFilter::Runtime(
      std::make_shared<pag::GlowMergeRuntimeFilter>(nullptr, _glowThreshold, std::move(blurImage)));
  return input->makeWithFilter(std::move(mergeFilter), offset);
}

}  // namespace pagx
