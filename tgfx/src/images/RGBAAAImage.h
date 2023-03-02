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

#pragma once

#include "tgfx/core/Image.h"

namespace tgfx {
class RGBAAAImage : public Image {
 public:
  int width() const override {
    return bounds.width();
  }

  int height() const override {
    return bounds.height();
  }

  bool isRGBAAA() const override {
    return true;
  }

 protected:
  std::shared_ptr<Image> onCloneWithSource(std::shared_ptr<ImageSource> newSource) const override;

  std::shared_ptr<Image> onMakeSubset(const Rect& subset) const override;

  std::shared_ptr<Image> onMakeRGBAAA(int displayWidth, int displayHeight, int alphaStartX,
                                      int alphaStartY) const override;

  std::unique_ptr<FragmentProcessor> asFragmentProcessor(Context* context, TileMode tileModeX,
                                                         TileMode tileModeY,
                                                         const SamplingOptions& sampling,
                                                         const Matrix* localMatrix = nullptr,
                                                         uint32_t surfaceFlags = 0) override;

 private:
  Rect bounds = Rect::MakeEmpty();
  Point alphaStart = Point::Zero();

  RGBAAAImage(std::shared_ptr<ImageSource> source, int displayWidth, int displayHeight,
              int alphaStartX, int alphaStartY);

  RGBAAAImage(std::shared_ptr<ImageSource> source, const Rect& bounds, const Point& alphaStart);

  friend class Image;
};
}  // namespace tgfx
