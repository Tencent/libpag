/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "OuterGlowFilter.h"
#include <tgfx/core/Recorder.h>
#include "base/utils/TGFXCast.h"
#include "rendering/filters/utils/BlurTypes.h"
#include "tgfx/core/ImageFilter.h"

namespace pag {
OuterGlowFilter::OuterGlowFilter(OuterGlowStyle* layerStyle) : layerStyle(layerStyle) {
}

bool OuterGlowFilter::draw(Frame layerFrame, std::shared_ptr<tgfx::Image> source,
                           const tgfx::Point& filterScale, const tgfx::Matrix& matrix,
                           tgfx::Canvas* target) {
  auto spread = layerStyle->spread->getValueAt(layerFrame);
  auto color = ToTGFX(layerStyle->color->getValueAt(layerFrame));
  auto opacity = ToAlpha(layerStyle->opacity->getValueAt(layerFrame));
  auto size = layerStyle->size->getValueAt(layerFrame);
  auto range = layerStyle->range->getValueAt(layerFrame);
  spread *= (spread == 1.f) ? 1.f : 0.8f;
  auto spreadSize = size * spread / range;
  auto blurSize = size * (1.f - spread) * 2.f / range;
  auto blurXSize = blurSize * filterScale.x;
  auto blurYSize = blurSize * filterScale.y;

  auto strokeOption = SolidStrokeOption();
  strokeOption.color = layerStyle->color->getValueAt(layerFrame);
  strokeOption.opacity = layerStyle->opacity->getValueAt(layerFrame);
  strokeOption.spreadSizeX = spreadSize;
  strokeOption.spreadSizeY = spreadSize;

  if (spread == 0.f) {
    tgfx::Paint paint;
    paint.setImageFilter(createBlurFilter(blurXSize, blurYSize, color, filterScale));
    paint.setAlpha(opacity);
    target->drawImage(source, &paint);
    return true;
  }

  if (spread == 1.f) {
    auto strokeFilter = SolidStrokeEffect::CreateFilter(strokeOption, filterScale);
    if (strokeFilter == nullptr) {
      return false;
    }
    tgfx::Paint paint;
    paint.setImageFilter(strokeFilter);
    target->drawImage(source, matrix, &paint);
    return true;
  }
  auto strokeFilter = SolidStrokeEffect::CreateFilter(strokeOption, filterScale);
  if (strokeFilter == nullptr) {
    return false;
  }

  auto dropShadowFilter = createBlurFilter(blurXSize, blurYSize, color, filterScale);
  auto composeFilter = tgfx::ImageFilter::Compose(strokeFilter, dropShadowFilter);
  tgfx::Paint paint;
  paint.setImageFilter(composeFilter);
  paint.setAlpha(opacity);
  target->drawImage(source, matrix, &paint);
  return true;
}

std::shared_ptr<tgfx::ImageFilter> OuterGlowFilter::createBlurFilter(
    float blurrinessX, float blurrinessY, const tgfx::Color& color,
    const tgfx::Point& filterScale) {
  return tgfx::ImageFilter::DropShadowOnly(0, 0, blurrinessX * filterScale.x,
                                           blurrinessY * filterScale.y, color);
}

}  // namespace pag
