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

#include "StrokeFilter.h"
#include <tgfx/core/Canvas.h>
#include <tgfx/core/Paint.h>
#include "tgfx/core/ImageFilter.h"

namespace pag {
StrokeFilter::StrokeFilter(StrokeStyle* layerStyle) : layerStyle(layerStyle) {
}

bool StrokeFilter::draw(Frame layerFrame, std::shared_ptr<tgfx::Image> source,
                        const tgfx::Point& filterScale, const tgfx::Matrix& matrix,
                        tgfx::Canvas* target) {

  if (source == nullptr) {
    return false;
  }
  auto strokePosition = layerStyle->position->getValueAt(layerFrame);
  auto spreadSize = layerStyle->size->getValueAt(layerFrame);

  auto strokeOption = SolidStrokeOption();
  strokeOption.color = layerStyle->color->getValueAt(layerFrame);
  strokeOption.opacity = layerStyle->opacity->getValueAt(layerFrame);
  strokeOption.spreadSizeX = spreadSize;
  strokeOption.spreadSizeY = spreadSize;
  strokeOption.position = strokePosition;

  if (strokePosition == StrokePosition::Center) {
    strokeOption.spreadSizeX *= 0.4;
    strokeOption.spreadSizeY *= 0.4;
  } else if (strokePosition == StrokePosition::Inside) {
    strokeOption.spreadSizeX *= 0.8;
    strokeOption.spreadSizeY *= 0.8;
  }

  if (strokePosition == StrokePosition::Outside) {
    auto strokeFilter = SolidStrokeEffect::CreateFilter(strokeOption, filterScale, nullptr);
    if (strokeFilter == nullptr) {
      return false;
    }
    tgfx::Paint paint;
    paint.setImageFilter(strokeFilter);
    target->drawImage(source, matrix, &paint);
    return true;
  }

  auto alphaEdgeDetectFilter =
      tgfx::ImageFilter::Runtime(std::make_shared<AlphaEdgeDetectLayerEffect>());
  auto strokeFilter = SolidStrokeEffect::CreateFilter(strokeOption, filterScale, source);
  auto composeFilter = tgfx::ImageFilter::Compose(alphaEdgeDetectFilter, strokeFilter);
  if (composeFilter == nullptr) {
    return false;
  }
  tgfx::Paint paint;
  paint.setImageFilter(composeFilter);
  target->drawImage(source, matrix, &paint);
  return true;
}
}  // namespace pag
