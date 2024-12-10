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
#include "rendering/filters/effects/SolidStrokeEffect.h"
#include "rendering/filters/utils/BlurTypes.h"
#include "tgfx/core/ImageFilter.h"

namespace pag {
OuterGlowFilter::OuterGlowFilter(OuterGlowStyle* layerStyle) : layerStyle(layerStyle) {
}

void OuterGlowFilter::update(Frame layerFrame, const tgfx::Point& filterScale) {
  spread = layerStyle->spread->getValueAt(layerFrame);
  spread *= (spread == 1.f) ? 1.f : 0.8f;
  color = ToTGFX(layerStyle->color->getValueAt(layerFrame));
  alpha = ToAlpha(layerStyle->opacity->getValueAt(layerFrame));
  auto size = layerStyle->size->getValueAt(layerFrame);
  sizeX = size * filterScale.x;
  sizeY = size * filterScale.y;
  mode = size < STROKE_SPREAD_MIN_THICK_SIZE ? SolidStrokeMode::Normal : SolidStrokeMode::Thick;
  range = layerStyle->range->getValueAt(layerFrame);
}

bool OuterGlowFilter::draw(tgfx::Canvas* canvas, std::shared_ptr<tgfx::Image> image) {
  std::shared_ptr<tgfx::ImageFilter> filter = nullptr;
  if (spread == 0.f) {
    filter = getDropShadowFilter();
  } else if (spread == 1.f) {
    filter = getStrokeFilter();
  } else {
    auto strokeFilter = getStrokeFilter();
    if (strokeFilter == nullptr) {
      return false;
    }
    auto dropShadowFilter = getDropShadowFilter();
    if (dropShadowFilter == nullptr) {
      return false;
    }
    filter = tgfx::ImageFilter::Compose(strokeFilter, dropShadowFilter);
  }
  if (!filter) {
    return false;
  }
  tgfx::Paint paint;
  paint.setImageFilter(filter);
  paint.setAlpha(alpha);
  canvas->drawImage(std::move(image), &paint);
  return true;
}

std::shared_ptr<tgfx::ImageFilter> OuterGlowFilter::getStrokeFilter() const {
  auto strokeOption = SolidStrokeOption();
  strokeOption.color = color;
  strokeOption.spreadSizeX = spread * sizeX / range;
  strokeOption.spreadSizeY = spread * sizeY / range;
  return SolidStrokeEffect::CreateFilter(strokeOption, mode);
}

std::shared_ptr<tgfx::ImageFilter> OuterGlowFilter::getDropShadowFilter() const {
  auto blurSizeX = sizeX * (1.f - spread) * 2.f / range;
  auto blurSizeY = sizeY * (1.f - spread) * 2.f / range;
  return tgfx::ImageFilter::DropShadowOnly(0, 0, blurSizeX, blurSizeY, color);
}

}  // namespace pag
