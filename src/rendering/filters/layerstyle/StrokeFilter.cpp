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
#include "rendering/filters/utils/BlurTypes.h"
#include "tgfx/core/ImageFilter.h"

namespace pag {
StrokeFilter::StrokeFilter(StrokeStyle* layerStyle) : layerStyle(layerStyle) {
  strokeFilter = new SolidStrokeFilter();
  alphaEdgeDetectFilter = new AlphaEdgeDetectFilter();
}

StrokeFilter::~StrokeFilter() {
  delete strokeFilter;
  delete alphaEdgeDetectFilter;
}

std::shared_ptr<tgfx::Image> StrokeFilter::applyFilterEffect(Frame layerFrame,
                                                             std::shared_ptr<tgfx::Image> source,
                                                             const tgfx::Point& filterScale,
                                                             tgfx::Point* offset) {

  if (source == nullptr) {
    return nullptr;
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

  strokeFilter->setSolidStrokeMode(
      spreadSize < STROKE_SPREAD_MIN_THICK_SIZE ? SolidStrokeMode::Normal : SolidStrokeMode::Thick);
  strokeFilter->onUpdateOption(strokeOption);

  if (strokePosition == StrokePosition::Outside) {
    return strokeFilter->applyFilterEffect(layerFrame, source, filterScale, offset);
  }

  tgfx::Point totalOffset = tgfx::Point::Zero();
  auto image =
      alphaEdgeDetectFilter->applyFilterEffect(layerFrame, source, filterScale, &totalOffset);
  if (image == nullptr) {
    return nullptr;
  }
  strokeFilter->onUpdateOriginalImage(source);
  image = strokeFilter->applyFilterEffect(layerFrame, image, filterScale, offset);
  if (offset) {
    *offset += totalOffset;
  }
  return image;
}
}  // namespace pag
