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
#include "gpu/FragmentProcessor.h"

namespace pag {
struct FPArgs {
  FPArgs(Context* context, const Matrix& localMatrix) : context(context), localMatrix(localMatrix) {
  }

  Context* context = nullptr;
  Matrix localMatrix = Matrix::I();
};

class Shader {
 public:
  static std::unique_ptr<Shader> MakeColorShader(Color color, Opacity opacity = Opaque);

  virtual ~Shader() = default;

  virtual std::unique_ptr<FragmentProcessor> asFragmentProcessor(const FPArgs& args) const = 0;
};

class Color4Shader : public Shader {
 public:
  explicit Color4Shader(Color4f color) : color(color) {
  }

  std::unique_ptr<FragmentProcessor> asFragmentProcessor(const FPArgs& args) const override;

 private:
  Color4f color;
};

class GradientShaderBase : public Shader {
 public:
  GradientShaderBase(const std::vector<Color4f>& colors, const std::vector<float>& positions,
                     const Matrix& pointsToUnit);

  std::vector<Color4f> originalColors = {};
  std::vector<float> originalPositions = {};

 protected:
  const Matrix pointsToUnit;
};

class LinearGradient : public GradientShaderBase {
 public:
  LinearGradient(const Point& startPoint, const Point& endPoint, const std::vector<Color4f>& colors,
                 const std::vector<float>& positions);

  std::unique_ptr<FragmentProcessor> asFragmentProcessor(const FPArgs& args) const override;
};

class RadialGradient : public GradientShaderBase {
 public:
  RadialGradient(const Point& center, float radius, const std::vector<Color4f>& colors,
                 const std::vector<float>& positions);

  std::unique_ptr<FragmentProcessor> asFragmentProcessor(const FPArgs& args) const override;
};

class GradientShader {
 public:
  static std::unique_ptr<Shader> MakeLinear(const Point& startPoint, const Point& endPoint,
                                            const std::vector<Color4f>& colors,
                                            const std::vector<float>& positions);

  static std::unique_ptr<Shader> MakeRadial(const Point& center, float radius,
                                            const std::vector<Color4f>& colors,
                                            const std::vector<float>& positions);
};
}  // namespace pag
