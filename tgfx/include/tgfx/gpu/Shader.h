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

#include <memory>
#include "tgfx/core/Color.h"
#include "tgfx/core/Point.h"

namespace tgfx {
struct FPArgs;

class FragmentProcessor;

/**
 * Shaders specify the source color(s) for what is being drawn. If a paint has no shader, then the
 * paint's color is used. If the paint has a shader, then the shader's color(s) are use instead, but
 * they are modulated by the paint's alpha.
 */
class Shader {
 public:
  /**
   * Create a shader that draws the specified color.
   */
  static std::shared_ptr<Shader> MakeColorShader(Color color);

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
  static std::shared_ptr<Shader> MakeLinearGradient(const Point& startPoint, const Point& endPoint,
                                                    const std::vector<Color>& colors,
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
  static std::shared_ptr<Shader> MakeRadialGradient(const Point& center, float radius,
                                                    const std::vector<Color>& colors,
                                                    const std::vector<float>& positions);

  virtual ~Shader() = default;

  /**
   * Returns true if the shader is guaranteed to produce only opaque colors, subject to the Paint
   * using the shader to apply an opaque alpha value. Subclasses should override this to allow some
   * optimizations.
   */
  virtual bool isOpaque() const {
    return false;
  }

  virtual std::unique_ptr<FragmentProcessor> asFragmentProcessor(const FPArgs& args) const = 0;
};
}  // namespace tgfx
