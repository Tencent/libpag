/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "GlowFilter.h"
#include "GlowBlurFilter.h"
#include "GlowMergeFilter.h"
#include "base/utils/Log.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "tgfx/core/Canvas.h"

namespace pag {

std::shared_ptr<tgfx::Image> GlowFilter::Apply(std::shared_ptr<tgfx::Image> input,
                                               RenderCache* cache, Effect* effect,
                                               Frame layerFrame, tgfx::Point* offset) {
  auto glowEffect = static_cast<GlowEffect*>(effect);
  auto glowRadius = glowEffect->glowRadius->getValueAt(layerFrame);
  auto resizeRatio = 1.0f - glowRadius / 1500.f;
  auto progress = glowEffect->glowThreshold->getValueAt(layerFrame);
  auto blurWidth = ceil(input->width() * resizeRatio);
  auto blurHeight = ceil(input->height() * resizeRatio);
  auto blurFilterH = std::make_shared<GlowBlurRuntimeFilter>(cache, BlurDirection::Horizontal,
                                                             1.0f / blurWidth, resizeRatio);
  auto imageFilterH = tgfx::ImageFilter::Runtime(blurFilterH);
  auto blurFilterV =
      std::make_shared<GlowBlurRuntimeFilter>(cache, BlurDirection::Vertical, 1.0f / blurHeight, 1.0f);
  auto imageFilterV = tgfx::ImageFilter::Runtime(blurFilterV);
  auto blurFilter = tgfx::ImageFilter::Compose(imageFilterH, imageFilterV);
  auto blurImage = input->makeWithFilter(std::move(blurFilter));
  auto mergeFilter =
      tgfx::ImageFilter::Runtime(std::make_shared<GlowMergeRuntimeFilter>(cache, progress, blurImage));
  return input->makeWithFilter(std::move(mergeFilter), offset);
}
}  // namespace pag
