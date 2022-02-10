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

#include "core/Color4f.h"
#include "gpu/Shader.h"

namespace pag {
/**
 * GradientShader hosts factories for creating subclasses of Shader that render linear and radial
 * gradients.
 */
class GradientShader {
 public:
  /**
   * Returns a shader that generates a linear gradient between the two specified points.
   * @param startPoint The start point for the gradient.
   * @param endPoint The end point for the gradient.
   * @param colors The array of colors, to be distributed between the two points.
   * @param positions May be empty. The relative position of each corresponding color in the colors
   * array. If this is empty, the the colors are distributed evenly between the start and end point.
   * If this is not empty, the values must begin with 0, end with 1.0, and intermediate values must
   * be strictly increasing.
   */
  static std::shared_ptr<Shader> MakeLinear(const Point& startPoint, const Point& endPoint,
                                            const std::vector<Color4f>& colors,
                                            const std::vector<float>& positions);

  /**
   * Returns a shader that generates a radial gradient given the center and radius.
   * @param center The center of the circle for this gradient
   * @param radius Must be positive. The radius of the circle for this gradient.
   * @param colors The array of colors, to be distributed between the center and edge of the circle.
   * @param positions May be empty. The relative position of each corresponding color in the colors
   * array. If this is empty, the the colors are distributed evenly between the start and end point.
   * If this is not empty, the values must begin with 0, end with 1.0, and intermediate values must
   * be strictly increasing.
   */
  static std::shared_ptr<Shader> MakeRadial(const Point& center, float radius,
                                            const std::vector<Color4f>& colors,
                                            const std::vector<float>& positions);
};
}  // namespace pag
