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
#include "rendering/filters/utils/FilterHelper.h"

namespace pag {
bool GaussianBlurFilter::initialize(tgfx::Context*) {
  return true;
}

void GaussianBlurFilter::draw(tgfx::Context* context, const FilterSource* source,
                              const FilterTarget* target) {
  if (source == nullptr || target == nullptr) {
    LOGE("GaussianBlurFilter::draw() can not draw filter");
    return;
  }
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
  blurrinessX *= filterScale.x * source->scale.x;
  blurrinessY *= filterScale.y * source->scale.y;
  tgfx::TileMode tileMode = tgfx::TileMode::Decal;
  tgfx::Rect cropRect = tgfx::Rect::MakeEmpty();
  if (repeatEdgePixels) {
    tileMode = tgfx::TileMode::Clamp;
    cropRect = tgfx::Rect::MakeWH(source->width, source->height);
  }
  auto blurFilter = tgfx::ImageFilter::Blur(blurrinessX, blurrinessY, tileMode, cropRect);
  tgfx::BackendRenderTarget renderTarget = {target->frameBuffer, target->width, target->height};
  auto targetSurface = tgfx::Surface::MakeFrom(context, renderTarget, tgfx::SurfaceOrigin::TopLeft);
  auto targetCanvas = targetSurface->getCanvas();
  tgfx::BackendTexture backendTexture = {source->sampler, source->width, source->height};
  auto image = tgfx::Image::MakeFrom(context, backendTexture);
  targetCanvas->save();
  targetCanvas->setMatrix(ToMatrix(target));
  tgfx::Paint paint;
  paint.setImageFilter(blurFilter);
  targetCanvas->drawImage(std::move(image), &paint);
  targetCanvas->restore();
  targetCanvas->flush();
}
}  // namespace pag
