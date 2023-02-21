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

#include "tgfx/core/ImageOrigin.h"

namespace tgfx {
Matrix ImageOriginToMatrix(ImageOrigin origin, int width, int height) {
  auto w = static_cast<float>(width);
  auto h = static_cast<float>(height);
  switch (origin) {
    case ImageOrigin::TopLeft:
      return Matrix::I();
    case ImageOrigin::TopRight:
      return Matrix::MakeAll(-1, 0, w, 0, 1, 0, 0, 0, 1);
    case ImageOrigin::BottomRight:
      return Matrix::MakeAll(-1, 0, w, 0, -1, h, 0, 0, 1);
    case ImageOrigin::BottomLeft:
      return Matrix::MakeAll(1, 0, 0, 0, -1, h, 0, 0, 1);
    case ImageOrigin::LeftTop:
      return Matrix::MakeAll(0, 1, 0, 1, 0, 0, 0, 0, 1);
    case ImageOrigin::RightTop:
      return Matrix::MakeAll(0, -1, h, 1, 0, 0, 0, 0, 1);
    case ImageOrigin::RightBottom:
      return Matrix::MakeAll(0, -1, h, -1, 0, w, 0, 0, 1);
    case ImageOrigin::LeftBottom:
      return Matrix::MakeAll(0, 1, 0, -1, 0, w, 0, 0, 1);
  }
  return Matrix::I();
}

void ApplyImageOrigin(ImageOrigin origin, int* width, int* height) {
  if (origin == ImageOrigin::LeftTop || origin == ImageOrigin::RightTop ||
      origin == ImageOrigin::RightBottom || origin == ImageOrigin::LeftBottom) {
    std::swap(*width, *height);
  }
}
}  // namespace tgfx