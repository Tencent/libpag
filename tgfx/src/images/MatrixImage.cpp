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

#include "MatrixImage.h"
#include "ImageSource.h"

namespace tgfx {
MatrixImage::MatrixImage(std::shared_ptr<ImageSource> imageSource, int width, int height,
                         const Matrix& localMatrix)
    : Image(std::move(imageSource)), _width(width), _height(height), localMatrix(localMatrix) {
}

std::shared_ptr<Image> MatrixImage::onCloneWithSource(
    std::shared_ptr<ImageSource> newSource) const {
  return std::shared_ptr<MatrixImage>(
      new MatrixImage(std::move(newSource), _width, _height, localMatrix));
}

std::shared_ptr<Image> MatrixImage::onMakeSubset(const Rect& subset) const {
  auto matrix = Matrix::MakeTrans(subset.x(), subset.y());
  matrix.postConcat(localMatrix);
  return std::shared_ptr<MatrixImage>(new MatrixImage(source, static_cast<int>(subset.width()),
                                                      static_cast<int>(subset.height()), matrix));
}

std::shared_ptr<Image> MatrixImage::onMakeRGBAAA(int, int, int, int) const {
  return nullptr;
}

std::unique_ptr<FragmentProcessor> MatrixImage::asFragmentProcessor(
    Context* context, uint32_t surfaceFlags, TileMode tileModeX, TileMode tileModeY,
    const SamplingOptions& sampling, const Matrix* extraMatrix) {
  auto matrix = localMatrix;
  if (extraMatrix != nullptr) {
    matrix.postConcat(*extraMatrix);
  }
  return Image::asFragmentProcessor(context, surfaceFlags, tileModeX, tileModeY, sampling, &matrix);
}
}  // namespace tgfx
