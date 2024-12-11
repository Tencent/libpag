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

#include "DropShadowFilter.h"
#include "base/utils/MathUtil.h"
#include "base/utils/TGFXCast.h"
#include "rendering/filters/layerstyle/SolidStrokeFilter.h"
#include "rendering/filters/utils/BlurTypes.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/ImageFilter.h"

namespace pag {
DropShadowFilter::DropShadowFilter(DropShadowStyle* layerStyle) : layerStyle(layerStyle) {
}

void DropShadowFilter::update(Frame layerFrame, const tgfx::Point& filterScale,
                              const tgfx::Point& sourceScale) {
  auto totalScale = tgfx::Point::Make(filterScale.x * sourceScale.x, filterScale.y * sourceScale.y);
  spread = layerStyle->spread->getValueAt(layerFrame);
  spread *= (spread == 1.f) ? 1.f : 0.8f;
  color = ToTGFX(layerStyle->color->getValueAt(layerFrame));
  alpha = ToAlpha(layerStyle->opacity->getValueAt(layerFrame));
  auto size = layerStyle->size->getValueAt(layerFrame);
  sizeX = size * totalScale.x;
  sizeY = size * totalScale.y;
  mode = size * spread < STROKE_SPREAD_MIN_THICK_SIZE ? SolidStrokeMode::Normal
                                                      : SolidStrokeMode::Thick;
  auto distance = layerStyle->distance->getValueAt(layerFrame);
  if (distance > 0.f) {
    auto angle = layerStyle->angle->getValueAt(layerFrame);
    auto radians = DegreesToRadians(angle - 180.f);
    offsetX = cosf(radians) * distance * totalScale.x;
    offsetY = -sinf(radians) * distance * totalScale.y;
  } else {
    offsetX = 0.f;
    offsetY = 0.f;
  }
}

bool DropShadowFilter::draw(tgfx::Canvas* canvas, std::shared_ptr<tgfx::Image> image) {
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

std::shared_ptr<tgfx::ImageFilter> DropShadowFilter::getStrokeFilter() const {
  auto strokeOption = SolidStrokeOption();
  strokeOption.color = color;
  strokeOption.spreadSizeX = sizeX * spread;
  strokeOption.spreadSizeY = sizeY * spread;
  strokeOption.offsetX = offsetX;
  strokeOption.offsetY = offsetY;
  return SolidStrokeFilter::CreateFilter(strokeOption, mode);
}

std::shared_ptr<tgfx::ImageFilter> DropShadowFilter::getDropShadowFilter() const {
  float blurSizeX = sizeX * (1.f - spread) * 2.f;
  float blurSizeY = sizeY * (1.f - spread) * 2.f;
  return tgfx::ImageFilter::DropShadowOnly(offsetX, offsetY, blurSizeX, blurSizeY, color);
}

}  // namespace pag
