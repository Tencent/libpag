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

#include "DropShadowFilter.h"
#include "base/utils/MathUtil.h"
#include "base/utils/TGFXCast.h"
#include "rendering/filters/layerstyle/SolidStrokeFilter.h"
#include "rendering/filters/utils/BlurTypes.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/ImageFilter.h"

namespace pag {
DropShadowFilter::DropShadowFilter(DropShadowStyle* layerStyle, RenderCache* cache)
    : layerStyle(layerStyle) {
  this->cache = cache;
}

void DropShadowFilter::update(Frame layerFrame, const tgfx::Point& filterScale,
                              const tgfx::Point& sourceScale) {
  auto totalScale = tgfx::Point::Make(filterScale.x * sourceScale.x, filterScale.y * sourceScale.y);
  spread = layerStyle->spread->getValueAt(layerFrame);
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

bool DropShadowFilter::draw(Canvas* canvas, std::shared_ptr<tgfx::Image> image) {
  std::shared_ptr<tgfx::ImageFilter> filter = nullptr;
  if (spread == 0.f) {
    filter = getDropShadowFilter(offsetX, offsetY);
  } else if (spread == 1.f) {
    filter = getStrokeFilter();
  } else {
    auto strokeFilter = getStrokeFilter();
    if (strokeFilter == nullptr) {
      return false;
    }
    auto dropShadowFilter = getDropShadowFilter(0, 0);
    if (dropShadowFilter == nullptr) {
      return false;
    }
    filter = tgfx::ImageFilter::Compose(strokeFilter, dropShadowFilter);
  }
  if (!filter) {
    return false;
  }
  tgfx::Point point;
  image = image->makeWithFilter(filter, &point);
  tgfx::Paint paint;
  paint.setAlpha(alpha);
  canvas->drawImage(std::move(image), point.x, point.y, &paint);
  return true;
}

std::shared_ptr<tgfx::ImageFilter> DropShadowFilter::getStrokeFilter() const {
  auto strokeOption = SolidStrokeOption();
  strokeOption.color = color;
  strokeOption.spreadSizeX = sizeX * spread;
  strokeOption.spreadSizeY = sizeY * spread;
  strokeOption.offsetX = offsetX;
  strokeOption.offsetY = offsetY;
  return SolidStrokeFilter::CreateFilter(cache, strokeOption, mode);
}

std::shared_ptr<tgfx::ImageFilter> DropShadowFilter::getDropShadowFilter(float offsetX,
                                                                         float offsetY) const {
  float blurSizeX = sizeX * (1.f - spread) / 2;
  float blurSizeY = sizeY * (1.f - spread) / 2;
  return tgfx::ImageFilter::DropShadowOnly(offsetX, offsetY, blurSizeX, blurSizeY, color);
}

}  // namespace pag
