/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
GlowFilter::GlowFilter(Effect* effect) : effect(effect) {
}

GlowFilter::~GlowFilter() {
}

void GlowFilter::update(Frame layerFrame, const tgfx::Point&) {
  auto glowEffect = static_cast<GlowEffect*>(effect);
  auto glowRadius = glowEffect->glowRadius->getValueAt(layerFrame);
  resizeRatio = 1.0f - glowRadius / 1500.f;
  progress = glowEffect->glowThreshold->getValueAt(layerFrame);
}

void GlowFilter::applyFilter(tgfx::Canvas* canvas, std::shared_ptr<tgfx::Image> image) {
  if (image == nullptr) {
    LOGE("GlowFilter::draw() can not draw filter");
    return;
  }
  auto blurWidth = ceil(image->width() * resizeRatio);
  auto blurHeight = ceil(image->height() * resizeRatio);
  auto blurFilterH = std::make_shared<GlowBlurRuntimeFilter>(BlurDirection::Horizontal,
                                                             1.0f / blurWidth, resizeRatio);
  auto imageFilterH = tgfx::ImageFilter::Runtime(blurFilterH);
  auto blurFilterV =
      std::make_shared<GlowBlurRuntimeFilter>(BlurDirection::Vertical, 1.0f / blurHeight, 1.0f);
  auto imageFilterV = tgfx::ImageFilter::Runtime(blurFilterV);
  auto blurFilter = tgfx::ImageFilter::Compose(imageFilterH, imageFilterV);
  auto blurImage = image->makeWithFilter(std::move(blurFilter));
  auto mergeFilter =
      tgfx::ImageFilter::Runtime(std::make_shared<GlowMergeRuntimeFilter>(progress, blurImage));

  tgfx::Paint paint;
  paint.setImageFilter(mergeFilter);
  canvas->drawImage(image, &paint);
}
}  // namespace pag
