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

#include "gpu/TextureProxy.h"
#include "tgfx/core/Matrix.h"

namespace tgfx {
struct CoordTransform {
  CoordTransform() = default;

  explicit CoordTransform(Matrix matrix, const TextureProxy* proxy = nullptr,
                          const Point& alphaStart = Point::Zero())
      : matrix(matrix), textureProxy(proxy), alphaStart(alphaStart) {
  }

  Matrix getTotalMatrix() const;

  Matrix matrix = Matrix::I();
  const TextureProxy* textureProxy = nullptr;
  // The alpha start point of the RGBAAA layout.
  Point alphaStart = Point::Zero();
};
}  // namespace tgfx
