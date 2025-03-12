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

#include "GaussianBlurFilter.h"
#include <tgfx/core/Canvas.h>
#include "base/utils/Log.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "tgfx/core/ImageFilter.h"

namespace pag {

void GaussianBlurFilter::update(Frame layerFrame, const tgfx::Point& sourceScale) {
  auto* blurEffect = static_cast<FastBlurEffect*>(effect);
  repeatEdgePixels = blurEffect->repeatEdgePixels->getValueAt(layerFrame);
  auto blurDimensions = blurEffect->blurDimensions->getValueAt(layerFrame);
  auto blurrinessX = blurEffect->blurriness->getValueAt(layerFrame);
  auto blurrinessY = blurrinessX;
  if (blurDimensions == BlurDimensionsDirection::Horizontal) {
    blurrinessY = 0;
  } else if (blurDimensions == BlurDimensionsDirection::Vertical) {
    blurrinessX = 0;
  }
  blurrinessX *= _filterScale.x * sourceScale.x;
  blurrinessY *= _filterScale.y * sourceScale.y;
  if (repeatEdgePixels) {
    currentFilter = tgfx::ImageFilter::Blur(blurrinessX, blurrinessY, tgfx::TileMode::Clamp);
  } else {
    currentFilter = tgfx::ImageFilter::Blur(blurrinessX, blurrinessY);
  }
}

void GaussianBlurFilter::applyFilter(tgfx::Canvas* canvas, std::shared_ptr<tgfx::Image> image) {
  if (currentFilter == nullptr) {
    return;
  }
  canvas->save();
  if (repeatEdgePixels) {
    canvas->clipRect(tgfx::Rect::MakeWH(image->width(), image->height()));
  }
  tgfx::Paint paint;
  paint.setImageFilter(currentFilter);
  canvas->drawImage(image, &paint);
  canvas->restore();
}

}  // namespace pag
