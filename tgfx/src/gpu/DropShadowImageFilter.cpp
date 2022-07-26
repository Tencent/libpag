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

#include "DropShadowImageFilter.h"
#include "tgfx/gpu/ColorFilter.h"
#include "tgfx/gpu/Surface.h"

namespace tgfx {
std::shared_ptr<ImageFilter> ImageFilter::DropShadow(float dx, float dy, float blurrinessX,
                                                     float blurrinessY, const Color& color,
                                                     const Rect& cropRect) {
  return std::make_shared<DropShadowImageFilter>(dx, dy, blurrinessX, blurrinessY, color, false,
                                                 cropRect);
}

std::shared_ptr<ImageFilter> ImageFilter::DropShadowOnly(float dx, float dy, float blurrinessX,
                                                         float blurrinessY, const Color& color,
                                                         const Rect& cropRect) {
  return std::make_shared<DropShadowImageFilter>(dx, dy, blurrinessX, blurrinessY, color, true,
                                                 cropRect);
}

std::pair<std::shared_ptr<Texture>, Point> DropShadowImageFilter::filterImage(
    const ImageFilterContext& context) {
  auto image = context.source;
  if (image == nullptr) {
    return {};
  }
  auto inputBounds =
      Rect::MakeWH(static_cast<float>(image->width()), static_cast<float>(image->height()));
  Rect dstBounds = Rect::MakeEmpty();
  if (!applyCropRect(inputBounds, &dstBounds, &context.clipBounds)) {
    return {};
  }
  if (!inputBounds.intersect(dstBounds)) {
    return {};
  }
  dstBounds.roundOut();
  auto dstOffset = Point::Make(dstBounds.x(), dstBounds.y());
  auto surface = Surface::Make(context.context, static_cast<int>(dstBounds.width()),
                               static_cast<int>(dstBounds.height()));
  if (surface == nullptr) {
    return {};
  }
  auto canvas = surface->getCanvas();
  Paint paint;
  paint.setImageFilter(ImageFilter::Blur(blurrinessX, blurrinessY));
  paint.setColorFilter(ColorFilter::Blend(color, BlendMode::SrcIn));
  canvas->concat(Matrix::MakeTrans(-dstOffset.x, -dstOffset.y));
  canvas->save();
  canvas->concat(Matrix::MakeTrans(dx, dy));
  canvas->drawTexture(image, &paint);
  canvas->restore();
  if (!shadowOnly) {
    canvas->drawTexture(image);
  }
  return {surface->getTexture(), dstOffset};
}

Rect DropShadowImageFilter::onFilterNodeBounds(const Rect& srcRect) const {
  auto dstRect = srcRect.makeOffset(dx, dy);
  return ImageFilter::Blur(blurrinessX, blurrinessY)->filterBounds(dstRect);
}
}  // namespace tgfx
