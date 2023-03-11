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

#include "tgfx/core/EncodedOrigin.h"

namespace tgfx {
Matrix EncodedOriginToMatrix(EncodedOrigin origin, int width, int height) {
  auto w = static_cast<float>(width);
  auto h = static_cast<float>(height);
  switch (origin) {
    case EncodedOrigin::TopLeft:
      return Matrix::I();
    case EncodedOrigin::TopRight:
      return Matrix::MakeAll(-1, 0, w, 0, 1, 0, 0, 0, 1);
    case EncodedOrigin::BottomRight:
      return Matrix::MakeAll(-1, 0, w, 0, -1, h, 0, 0, 1);
    case EncodedOrigin::BottomLeft:
      return Matrix::MakeAll(1, 0, 0, 0, -1, h, 0, 0, 1);
    case EncodedOrigin::LeftTop:
      return Matrix::MakeAll(0, 1, 0, 1, 0, 0, 0, 0, 1);
    case EncodedOrigin::RightTop:
      return Matrix::MakeAll(0, -1, h, 1, 0, 0, 0, 0, 1);
    case EncodedOrigin::RightBottom:
      return Matrix::MakeAll(0, -1, h, -1, 0, w, 0, 0, 1);
    case EncodedOrigin::LeftBottom:
      return Matrix::MakeAll(0, 1, 0, -1, 0, w, 0, 0, 1);
  }
  return Matrix::I();
}
}  // namespace tgfx