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

#include "SubsetImage.h"
#include "ImageSource.h"

namespace tgfx {
static const auto TopLeftMatrix = Matrix::I();
static const auto TopRightMatrix = Matrix::MakeAll(-1, 0, 1, 0, 1, 0, 0, 0, 1);
static const auto BottomRightMatrix = Matrix::MakeAll(-1, 0, 1, 0, -1, 1, 0, 0, 1);
static const auto BottomLeftMatrix = Matrix::MakeAll(1, 0, 0, 0, -1, 1, 0, 0, 1);
static const auto LeftTopMatrix = Matrix::MakeAll(0, 1, 0, 1, 0, 0, 0, 0, 1);
static const auto RightTopMatrix = Matrix::MakeAll(0, -1, 1, 1, 0, 0, 0, 0, 1);
static const auto RightBottomMatrix = Matrix::MakeAll(0, -1, 1, -1, 0, 1, 0, 0, 1);
static const auto LeftBottomMatrix = Matrix::MakeAll(0, 1, 0, -1, 0, 1, 0, 0, 1);

static Matrix OriginToMatrix(EncodedOrigin origin) {
  switch (origin) {
    case EncodedOrigin::TopRight:
      return TopRightMatrix;
    case EncodedOrigin::BottomRight:
      return BottomRightMatrix;
    case EncodedOrigin::BottomLeft:
      return BottomLeftMatrix;
    case EncodedOrigin::LeftTop:
      return LeftTopMatrix;
    case EncodedOrigin::RightTop:
      return RightTopMatrix;
    case EncodedOrigin::RightBottom:
      return RightBottomMatrix;
    case EncodedOrigin::LeftBottom:
      return LeftBottomMatrix;
    default:
      break;
  }
  return TopLeftMatrix;
}

static EncodedOrigin ConcatOrigin(EncodedOrigin oldOrigin, EncodedOrigin newOrigin) {
  auto oldMatrix = OriginToMatrix(oldOrigin);
  auto newMatrix = OriginToMatrix(newOrigin);
  oldMatrix.postConcat(newMatrix);
  if (oldMatrix == TopRightMatrix) {
    return EncodedOrigin::TopRight;
  }
  if (oldMatrix == BottomRightMatrix) {
    return EncodedOrigin::BottomRight;
  }
  if (oldMatrix == BottomLeftMatrix) {
    return EncodedOrigin::BottomLeft;
  }
  if (oldMatrix == LeftTopMatrix) {
    return EncodedOrigin::LeftTop;
  }
  if (oldMatrix == RightTopMatrix) {
    return EncodedOrigin::RightTop;
  }
  if (oldMatrix == RightBottomMatrix) {
    return EncodedOrigin::RightBottom;
  }
  if (oldMatrix == LeftBottomMatrix) {
    return EncodedOrigin::LeftBottom;
  }
  return EncodedOrigin::TopLeft;
}

SubsetImage::SubsetImage(std::shared_ptr<ImageSource> source, const Rect& bounds)
    : Image(std::move(source)), bounds(bounds) {
}

SubsetImage::SubsetImage(std::shared_ptr<ImageSource> imageSource, EncodedOrigin origin)
    : Image(std::move(imageSource)), origin(origin) {
  bounds = Rect::MakeWH(source->width(), source->height());
  auto matrix = EncodedOriginToMatrix(origin, source->width(), source->height());
  matrix.mapRect(&bounds);
}

SubsetImage::SubsetImage(std::shared_ptr<ImageSource> source, const Rect& bounds,
                         EncodedOrigin origin)
    : Image(std::move(source)), bounds(bounds), origin(origin) {
}

std::shared_ptr<SubsetImage> SubsetImage::onCloneWith(const Rect& newBounds,
                                                      EncodedOrigin newOrigin) const {
  return std::shared_ptr<SubsetImage>(new SubsetImage(source, newBounds, newOrigin));
}

std::shared_ptr<Image> SubsetImage::onCloneWith(std::shared_ptr<ImageSource> newSource) const {
  return std::shared_ptr<SubsetImage>(new SubsetImage(std::move(newSource), bounds, origin));
}

std::shared_ptr<Image> SubsetImage::onMakeSubset(const Rect& subset) const {
  auto newBounds = subset;
  newBounds.offset(bounds.x(), bounds.y());
  return onCloneWith(newBounds, origin);
}

std::shared_ptr<Image> SubsetImage::onApplyOrigin(EncodedOrigin encodedOrigin) const {
  auto width = source->width();
  auto height = source->height();
  if (origin == EncodedOrigin::LeftTop || origin == EncodedOrigin::RightTop ||
      origin == EncodedOrigin::RightBottom || origin == EncodedOrigin::LeftBottom) {
    std::swap(width, height);
  }
  auto matrix = EncodedOriginToMatrix(encodedOrigin, width, height);
  auto newBounds = matrix.mapRect(bounds);
  auto newOrigin = ConcatOrigin(origin, encodedOrigin);
  return onCloneWith(newBounds, newOrigin);
}

Matrix SubsetImage::getTotalMatrix(const Matrix* localMatrix) const {
  auto matrix = EncodedOriginToMatrix(origin, source->width(), source->height());
  matrix.postTranslate(-bounds.x(), -bounds.y());
  matrix.invert(&matrix);
  if (localMatrix != nullptr) {
    matrix.preConcat(*localMatrix);
  }
  return matrix;
}

std::unique_ptr<FragmentProcessor> SubsetImage::asFragmentProcessor(
    Context* context, uint32_t surfaceFlags, TileMode tileModeX, TileMode tileModeY,
    const SamplingOptions& sampling, const Matrix* localMatrix) {
  auto matrix = getTotalMatrix(localMatrix);
  return Image::asFragmentProcessor(context, surfaceFlags, tileModeX, tileModeY, sampling, &matrix);
}
}  // namespace tgfx
