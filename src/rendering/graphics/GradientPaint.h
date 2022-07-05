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

#include "pag/file.h"
#include "tgfx/core/Color.h"
#include "tgfx/core/Matrix.h"
#include "tgfx/core/Point.h"
#include "tgfx/gpu/Shader.h"

namespace pag {
/**
 * Defines attributes for drawing gradient colors.
 */
class GradientPaint {
 public:
  GradientPaint() = default;

  GradientPaint(Enum fillType, Point startPoint, Point endPoint,
                const GradientColorHandle& gradientColor, const tgfx::Matrix& matrix,
                bool reverse = false);

  std::shared_ptr<tgfx::Shader> getShader() const;

 private:
  Enum gradientType = GradientFillType::Linear;
  tgfx::Point startPoint = tgfx::Point::Zero();
  tgfx::Point endPoint = tgfx::Point::Zero();
  std::vector<tgfx::Color> colors;
  std::vector<float> positions;
  tgfx::Matrix matrix = tgfx::Matrix::I();
};
}  // namespace pag
