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

#include "gpu/processors/FragmentProcessor.h"
#include "tgfx/core/Image.h"

namespace tgfx {
class SubsetImage : public Image {
 public:
  SubsetImage(std::shared_ptr<ImageSource> source, const Rect& bounds);

  SubsetImage(std::shared_ptr<ImageSource> source, EncodedOrigin origin);

  int width() const override {
    return bounds.width();
  }

  int height() const override {
    return bounds.height();
  }

 protected:
  Rect bounds = Rect::MakeEmpty();
  EncodedOrigin origin = EncodedOrigin::TopLeft;

  SubsetImage(std::shared_ptr<ImageSource> source, const Rect& bounds, EncodedOrigin origin);

  virtual std::shared_ptr<SubsetImage> onCloneWith(const Rect& newBounds,
                                                   EncodedOrigin newOrigin) const;

  std::shared_ptr<Image> onCloneWith(std::shared_ptr<ImageSource> newSource) const override;

  std::shared_ptr<Image> onMakeSubset(const Rect& subset) const override;

  std::shared_ptr<Image> onMakeRGBAAA(int, int, int, int) const override {
    return nullptr;
  }

  std::shared_ptr<Image> onApplyOrigin(EncodedOrigin encodedOrigin) const override;

  std::unique_ptr<FragmentProcessor> asFragmentProcessor(
      Context* context, uint32_t surfaceFlags, TileMode tileModeX, TileMode tileModeY,
      const SamplingOptions& sampling, const Matrix* localMatrix = nullptr) override;

  Matrix getTotalMatrix(const Matrix* localMatrix) const;
};
}  // namespace tgfx
