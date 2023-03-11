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

#include "RGBAAAImage.h"
#include "ImageSource.h"
#include "gpu/TextureEffect.h"

namespace tgfx {
RGBAAAImage::RGBAAAImage(std::shared_ptr<ImageSource> source, int displayWidth, int displayHeight,
                         int alphaStartX, int alphaStartY)
    : SubsetImage(std::move(source), Rect::MakeWH(displayWidth, displayHeight)),
      alphaStart(Point::Make(alphaStartX, alphaStartY)) {
}

RGBAAAImage::RGBAAAImage(std::shared_ptr<ImageSource> source, const Rect& bounds,
                         EncodedOrigin origin, const Point& alphaStart)
    : SubsetImage(std::move(source), bounds, origin), alphaStart(alphaStart) {
}

std::shared_ptr<SubsetImage> RGBAAAImage::onCloneWith(const Rect& newBounds,
                                                      EncodedOrigin newOrigin) const {
  return std::shared_ptr<RGBAAAImage>(new RGBAAAImage(source, newBounds, newOrigin, alphaStart));
}

std::shared_ptr<Image> RGBAAAImage::onCloneWith(std::shared_ptr<ImageSource> newSource) const {
  return std::shared_ptr<Image>(new RGBAAAImage(std::move(newSource), bounds, origin, alphaStart));
}

std::unique_ptr<FragmentProcessor> RGBAAAImage::asFragmentProcessor(Context* context,
                                                                    uint32_t surfaceFlags, TileMode,
                                                                    TileMode,
                                                                    const SamplingOptions& sampling,
                                                                    const Matrix* localMatrix) {
  auto matrix = getTotalMatrix(localMatrix);
  return TextureEffect::MakeRGBAAA(source->lockTextureProxy(context, surfaceFlags), sampling,
                                   alphaStart, &matrix);
}
}  // namespace tgfx
