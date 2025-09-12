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

#include "GaussianBlurFilter.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/ImageFilter.h"

namespace pag {

std::shared_ptr<tgfx::Image> GaussianBlurFilter::Apply(std::shared_ptr<tgfx::Image> input,
                                                       Effect* effect, Frame layerFrame,
                                                       const tgfx::Point& filterScale,
                                                       const tgfx::Point& sourceScale,
                                                       tgfx::Point* offset) {
  auto* blurEffect = static_cast<FastBlurEffect*>(effect);
  auto repeatEdgePixels = blurEffect->repeatEdgePixels->getValueAt(layerFrame);
  auto blurDimensions = blurEffect->blurDimensions->getValueAt(layerFrame);
  auto blurrinessX = blurEffect->blurriness->getValueAt(layerFrame);
  auto blurrinessY = blurrinessX;
  if (blurDimensions == BlurDimensionsDirection::Horizontal) {
    blurrinessY = 0;
  } else if (blurDimensions == BlurDimensionsDirection::Vertical) {
    blurrinessX = 0;
  }
  blurrinessX *= filterScale.x * sourceScale.x;
  blurrinessY *= filterScale.y * sourceScale.y;
  std::shared_ptr<tgfx::ImageFilter> filter;
  if (repeatEdgePixels) {
    filter = tgfx::ImageFilter::Blur(blurrinessX / 2, blurrinessY / 2, tgfx::TileMode::Clamp);
    tgfx::Rect clipBounds = tgfx::Rect::MakeWH(input->width(), input->height());
    return input->makeWithFilter(filter, offset, &clipBounds);
  }
  filter = tgfx::ImageFilter::Blur(blurrinessX / 2, blurrinessY / 2);
  return input->makeWithFilter(filter, offset);
}

}  // namespace pag
