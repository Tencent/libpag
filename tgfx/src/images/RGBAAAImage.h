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

#include "SubsetImage.h"

namespace tgfx {
class RGBAAAImage : public SubsetImage {
 public:
  RGBAAAImage(std::shared_ptr<ImageSource> source, int displayWidth, int displayHeight,
              int alphaStartX, int alphaStartY);

  bool isRGBAAA() const override {
    return true;
  }

 protected:
  std::shared_ptr<SubsetImage> onCloneWith(const Rect& newBounds,
                                           EncodedOrigin newOrigin) const override;

  std::shared_ptr<Image> onCloneWith(std::shared_ptr<ImageSource> newSource) const override;

  std::unique_ptr<FragmentProcessor> asFragmentProcessor(
      Context* context, uint32_t surfaceFlags, TileMode tileModeX, TileMode tileModeY,
      const SamplingOptions& sampling, const Matrix* localMatrix = nullptr) override;

 private:
  Point alphaStart = Point::Zero();

  RGBAAAImage(std::shared_ptr<ImageSource> source, const Rect& bounds, EncodedOrigin origin,
              const Point& alphaStart);
};
}  // namespace tgfx
