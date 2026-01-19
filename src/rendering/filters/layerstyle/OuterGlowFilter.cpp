/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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
#include "base/utils/TGFXCast.h"
#include "rendering/filters/layerstyle/SolidStrokeFilter.h"
#include "rendering/filters/utils/BlurTypes.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/ImageFilter.h"

namespace pag {
OuterGlowFilter::OuterGlowFilter(OuterGlowStyle* layerStyle, RenderCache* cache)
    : layerStyle(layerStyle) {
  this->cache = cache;
}

void OuterGlowFilter::update(Frame layerFrame, const tgfx::Point& filterScale,
                             const tgfx::Point& sourceScale) {
  auto totalScale = tgfx::Point::Make(filterScale.x * sourceScale.x, filterScale.y * sourceScale.y);
  spread = layerStyle->spread->getValueAt(layerFrame);
  color = ToTGFX(layerStyle->color->getValueAt(layerFrame));
  alpha = ToAlpha(layerStyle->opacity->getValueAt(layerFrame));
  auto size = layerStyle->size->getValueAt(layerFrame);
  sizeX = size * totalScale.x;
  sizeY = size * totalScale.y;
  mode = size < STROKE_SPREAD_MIN_THICK_SIZE ? SolidStrokeMode::Normal : SolidStrokeMode::Thick;
  range = layerStyle->range->getValueAt(layerFrame);
}

bool OuterGlowFilter::draw(Canvas* canvas, std::shared_ptr<tgfx::Image> image) {
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
  tgfx::Point offset;
  image = image->makeWithFilter(filter, &offset);
  tgfx::Paint paint;
  paint.setAlpha(alpha);
  canvas->drawImage(std::move(image), offset.x, offset.y, &paint);
  return true;
}

std::shared_ptr<tgfx::ImageFilter> OuterGlowFilter::getStrokeFilter() const {
  auto strokeOption = SolidStrokeOption();
  strokeOption.color = color;
  strokeOption.spreadSizeX = spread * sizeX / range;
  strokeOption.spreadSizeY = spread * sizeY / range;
  return SolidStrokeFilter::CreateFilter(cache, strokeOption, mode);
}

std::shared_ptr<tgfx::ImageFilter> OuterGlowFilter::getDropShadowFilter() const {
  auto blurSizeX = sizeX * (1.f - spread) / range;
  auto blurSizeY = sizeY * (1.f - spread) / range;
  return tgfx::ImageFilter::DropShadowOnly(0, 0, blurSizeX / 2, blurSizeY / 2, color);
}

}  // namespace pag
