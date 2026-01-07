/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "FilterHelper.h"

namespace pag {
std::array<float, 9> ToShaderMatrix(const tgfx::Matrix& matrix) {
  float values[6];
  matrix.get6(values);
  return {values[0], values[3], 0, values[1], values[4], 0, values[2], values[5], 1};
}

std::array<float, 9> ToVertexMatrix(const tgfx::Matrix& matrix, int width, int height,
                                    tgfx::ImageOrigin origin) {
  auto result = matrix;
  auto w = static_cast<float>(width);
  auto h = static_cast<float>(height);
  tgfx::Matrix convertMatrix = {};
  // The following is equivalent：
  // convertMatrix.setTranslate(1.0f, 1.0f);
  // convertMatrix.postScale(width/2.0f, height/2.0f);
  convertMatrix.setAll(w * 0.5f, 0.0f, w * 0.5f, 0.0f, h * 0.5f, h * 0.5f);
  result.preConcat(convertMatrix);
  if (convertMatrix.invert(&convertMatrix)) {
    result.postConcat(convertMatrix);
  }
  if (origin == tgfx::ImageOrigin::BottomLeft) {
    result.postScale(1.0f, -1.0f);
  }
  return ToShaderMatrix(result);
}

std::array<float, 9> ToTextureMatrix(const tgfx::Matrix& matrix, int width, int height,
                                     tgfx::ImageOrigin origin) {
  if (matrix.isIdentity() && origin == tgfx::ImageOrigin::TopLeft) {
    return ToShaderMatrix(matrix);
  }
  auto result = matrix;
  tgfx::Matrix convertMatrix = {};
  convertMatrix.setScale(static_cast<float>(width), static_cast<float>(height));
  result.preConcat(convertMatrix);
  if (convertMatrix.invert(&convertMatrix)) {
    result.postConcat(convertMatrix);
  }
  if (origin == tgfx::ImageOrigin::BottomLeft) {
    result.postScale(1.0f, -1.0f);
    result.postTranslate(0.0f, 1.0f);
  }
  return ToShaderMatrix(result);
}

tgfx::Matrix ToMatrix(const std::array<float, 9>& values) {
  tgfx::Matrix result = {};
  result.setAll(values[0], values[3], values[6], values[1], values[4], values[7]);
  return result;
}

tgfx::Point ToTexturePoint(const tgfx::Texture* source, const tgfx::Point& texturePoint) {
  return {texturePoint.x / static_cast<float>(source->width()),
          texturePoint.y / static_cast<float>(source->height())};
}

tgfx::Point ToVertexPoint(const tgfx::Texture* target, const tgfx::Point& point) {
  return {2.0f * point.x / static_cast<float>(target->width()) - 1.0f,
          2.0f * point.y / static_cast<float>(target->height()) - 1.0f};
}

}  // namespace pag
