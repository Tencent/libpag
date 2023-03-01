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
    : Image(std::move(source)),
      bounds(Rect::MakeWH(displayWidth, displayHeight)),
      alphaStart(Point::Make(alphaStartX, alphaStartY)) {
}

RGBAAAImage::RGBAAAImage(std::shared_ptr<ImageSource> source, const Rect& bounds,
                         const Point& alphaStart)
    : Image(std::move(source)), bounds(bounds), alphaStart(alphaStart) {
}

std::shared_ptr<Image> RGBAAAImage::onCloneWithSource(
    std::shared_ptr<ImageSource> newSource) const {
  return std::shared_ptr<Image>(new RGBAAAImage(std::move(newSource), bounds, alphaStart));
}

std::shared_ptr<Image> RGBAAAImage::onMakeSubset(const Rect& subset) const {
  auto newBounds = subset;
  newBounds.offset(bounds.x(), bounds.y());
  return std::shared_ptr<Image>(new RGBAAAImage(source, newBounds, alphaStart));
}

std::shared_ptr<Image> RGBAAAImage::onMakeRGBAAA(int, int, int, int) const {
  return nullptr;
}

std::unique_ptr<FragmentProcessor> RGBAAAImage::asFragmentProcessor(Context* context, TileMode,
                                                                    TileMode,
                                                                    const SamplingOptions& sampling,
                                                                    const Matrix* localMatrix,
                                                                    bool skipGeneratingCache) {
  auto matrix = Matrix::MakeTrans(bounds.x(), bounds.y());
  if (localMatrix != nullptr) {
    matrix.postConcat(*localMatrix);
  }
  return TextureEffect::MakeRGBAAA(source->lockTextureProxy(context, skipGeneratingCache), sampling,
                                   alphaStart, &matrix);
}
}  // namespace tgfx
