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

#include "StrokeFilter.h"
#include "rendering/filters/layerstyle/AlphaEdgeDetectFilter.h"
#include "rendering/filters/layerstyle/SolidStrokeFilter.h"
#include "rendering/filters/utils/BlurTypes.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/ImageFilter.h"
#include "tgfx/core/Paint.h"

namespace pag {
StrokeFilter::StrokeFilter(StrokeStyle* layerStyle, RenderCache* cache) : layerStyle(layerStyle) {
  this->cache = cache;
}

void StrokeFilter::update(Frame layerFrame, const tgfx::Point& filterScale,
                          const tgfx::Point& sourceScale) {
  auto totalScale = tgfx::Point::Make(filterScale.x * sourceScale.x, filterScale.y * sourceScale.y);
  strokeOption = SolidStrokeOption();
  auto spreadSize = layerStyle->size->getValueAt(layerFrame);
  mode =
      spreadSize < STROKE_SPREAD_MIN_THICK_SIZE ? SolidStrokeMode::Normal : SolidStrokeMode::Thick;
  auto sizeX = spreadSize * totalScale.x;
  ;
  auto sizeY = spreadSize * totalScale.y;
  auto color = ToTGFX(layerStyle->color->getValueAt(layerFrame));
  auto position = layerStyle->position->getValueAt(layerFrame);
  strokeOption.color = color;
  strokeOption.spreadSizeX = sizeX;
  strokeOption.spreadSizeY = sizeY;
  strokeOption.position = position;

  if (position == StrokePosition::Center) {
    strokeOption.spreadSizeX *= 0.4;
    strokeOption.spreadSizeY *= 0.4;
  } else if (position == StrokePosition::Inside) {
    strokeOption.spreadSizeX *= 0.8;
    strokeOption.spreadSizeY *= 0.8;
  }

  alpha = ToAlpha(layerStyle->opacity->getValueAt(layerFrame));
}

bool StrokeFilter::draw(Canvas* canvas, std::shared_ptr<tgfx::Image> image) {

  std::shared_ptr<tgfx::ImageFilter> filter = nullptr;
  if (strokeOption.position == StrokePosition::Outside) {
    filter = SolidStrokeFilter::CreateFilter(cache, strokeOption, mode, nullptr);
  } else {
    auto strokeFilter = SolidStrokeFilter::CreateFilter(cache, strokeOption, mode, image);
    if (strokeFilter == nullptr) {
      return false;
    }
    auto alphaEdgeDetectFilter =
        tgfx::ImageFilter::Runtime(std::make_shared<AlphaEdgeDetectLayerEffect>(cache));
    filter = tgfx::ImageFilter::Compose(alphaEdgeDetectFilter, strokeFilter);
  }
  if (filter == nullptr) {
    return false;
  }
  tgfx::Point offset;
  image = image->makeWithFilter(filter, &offset);
  tgfx::Paint paint;
  paint.setAlpha(alpha);
  canvas->drawImage(image, offset.x, offset.y, &paint);
  return true;
}
}  // namespace pag
