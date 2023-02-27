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

#include "gpu/FragmentProcessor.h"
#include "tgfx/core/Image.h"

namespace tgfx {
class MatrixImage : public Image {
 public:
  int width() const override {
    return _width;
  }

  int height() const override {
    return _height;
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
                                                         bool skipGeneratingCache = false) override;

 private:
  int _width = 0;
  int _height = 0;
  Matrix localMatrix = Matrix::I();

  MatrixImage(std::shared_ptr<ImageSource> source, int width, int height,
              const Matrix& localMatrix);

  friend class Image;
};
}  // namespace tgfx
