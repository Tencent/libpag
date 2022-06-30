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

#include "GradientPaint.h"
#include "base/utils/Interpolate.h"
#include "base/utils/TGFXCast.h"

namespace pag {
void ConvertColorStop(const GradientColorHandle& gradientColor, std::vector<Color>& colorValues,
                      std::vector<float>& colorPositions) {
  auto colorStops = gradientColor->colorStops;
  for (size_t i = 0; i < colorStops.size(); i++) {
    const auto& stop = colorStops[i];
    colorValues.push_back(stop.color);
    colorPositions.push_back(stop.position);
    if (stop.midpoint != 0.5f && i < colorStops.size() - 1) {
      const auto& nextStop = colorStops[i + 1];
      auto midColor = Interpolate(stop.color, nextStop.color, 0.5f);
      colorValues.push_back(midColor);
      auto position = stop.position + (nextStop.position - stop.position) * stop.midpoint;
      colorPositions.push_back(position);
    }
  }
}

void ConvertAlphaStop(const GradientColorHandle& gradientColor, std::vector<Opacity>& alphaValues,
                      std::vector<float>& alphaPositions) {
  auto alphaStops = gradientColor->alphaStops;
  for (size_t i = 0; i < alphaStops.size(); i++) {
    const auto& stop = alphaStops[i];
    alphaValues.push_back(stop.opacity);
    alphaPositions.push_back(stop.position);
    if (stop.midpoint != 0.5f && i < alphaStops.size() - 1) {
      const auto& nextStop = alphaStops[i + 1];
      auto midAlpha = Interpolate(stop.opacity, nextStop.opacity, 0.5f);
      alphaValues.push_back(midAlpha);
      auto position = stop.position + (nextStop.position - stop.position) * stop.midpoint;
      alphaPositions.push_back(position);
    }
  }
}

GradientPaint::GradientPaint(Enum fillType, Point startPoint, Point endPoint,
                             const GradientColorHandle& gradientColor, const tgfx::Matrix& matrix,
                             bool reverse)
    : gradientType(fillType), startPoint(ToTGFX(startPoint)), endPoint(ToTGFX(endPoint)),
      matrix(matrix) {
  std::vector<Color> colorValues;
  std::vector<float> colorPositions;
  ConvertColorStop(gradientColor, colorValues, colorPositions);

  std::vector<Opacity> opacityValues;
  std::vector<float> alphaPositions;
  ConvertAlphaStop(gradientColor, opacityValues, alphaPositions);

  size_t colorIndex = 0;
  size_t opacityIndex = 0;
  while (colorIndex < colorValues.size() && opacityIndex < opacityValues.size()) {
    auto colorPosition = colorPositions[colorIndex];
    auto alphaPosition = alphaPositions[opacityIndex];
    if (colorPosition == alphaPosition) {
      positions.push_back(colorPosition);
      auto color4f = ToTGFX(colorValues[colorIndex++], opacityValues[opacityIndex++]);
      colors.push_back(color4f);
    } else if (colorPosition < alphaPosition) {
      positions.push_back(colorPosition);
      Opacity opacity;
      if (opacityIndex > 0) {
        auto lastPosition = alphaPositions[opacityIndex - 1];
        auto factor = (colorPosition - lastPosition) * 1.0f / (alphaPosition - lastPosition);
        opacity = Interpolate(opacityValues[opacityIndex - 1], opacityValues[opacityIndex], factor);
      } else {
        opacity = opacityValues[opacityIndex];
      }
      auto color4f = ToTGFX(colorValues[colorIndex++], opacity);
      colors.push_back(color4f);
    } else {
      positions.push_back(alphaPosition);
      Color color = {};
      if (colorIndex > 0) {
        auto lastPosition = colorPositions[colorIndex - 1];
        auto factor = (alphaPosition - lastPosition) * 1.0f / (colorPosition - lastPosition);
        auto lastColor = colorValues[colorIndex - 1];
        auto currentColor = colorValues[colorIndex];
        color = Interpolate(lastColor, currentColor, factor);
      } else {
        color = colorValues[colorIndex];
      }
      auto color4f = ToTGFX(color, opacityValues[opacityIndex++]);
      colors.push_back(color4f);
    }
  }

  auto lastOpacity = opacityValues.back();
  while (colorIndex < colorValues.size()) {
    positions.push_back(colorPositions[colorIndex]);
    auto color4f = ToTGFX(colorValues[colorIndex++], lastOpacity);
    colors.push_back(color4f);
  }

  auto lastColor = colorValues.back();
  while (opacityIndex < opacityValues.size()) {
    positions.push_back(alphaPositions[opacityIndex]);
    auto color4f = ToTGFX(lastColor, opacityValues[opacityIndex++]);
    colors.push_back(color4f);
  }
  if (reverse) {
    std::swap(startPoint, endPoint);
    std::reverse(colors.begin(), colors.end());
    std::reverse(positions.begin(), positions.end());
    std::transform(positions.begin(), positions.end(), positions.begin(),
                   [](float x) { return 1.f - x; });
  }
}

std::shared_ptr<tgfx::Shader> GradientPaint::getShader() const {
  std::shared_ptr<tgfx::Shader> shader;
  if (gradientType == GradientFillType::Linear) {
    shader = tgfx::Shader::MakeLinearGradient(startPoint, endPoint, colors, positions);
  } else if (gradientType == GradientFillType::Radial) {
    auto radius = tgfx::Point::Distance(startPoint, endPoint);
    shader = tgfx::Shader::MakeRadialGradient(startPoint, radius, colors, positions);
  } else if (gradientType == GradientFillType::Angle) {
    auto center = startPoint + endPoint;
    center.set(center.x * 0.5f, center.y * 0.5f);
    shader = tgfx::Shader::MakeSweepGradient(center, 0, 360, colors, positions);
  } else if (gradientType == GradientFillType::Reflected) {
    auto reflectedColors = colors;
    reflectedColors.insert(reflectedColors.begin(), colors.rbegin(), colors.rend() - 1);
    auto reflectedPositions = positions;
    reflectedPositions.insert(reflectedPositions.begin(), positions.rbegin(), positions.rend() - 1);
    auto size = static_cast<int>(positions.size());
    std::transform(reflectedPositions.begin(), reflectedPositions.begin() + size,
                   reflectedPositions.begin(),
                   [](float position) { return (1.f - position) * 0.5; });
    std::transform(reflectedPositions.begin() + size, reflectedPositions.end(),
                   reflectedPositions.begin() + size,
                   [](float position) { return position * 0.5f + 0.5f; });
    shader =
        tgfx::Shader::MakeLinearGradient(startPoint, endPoint, reflectedColors, reflectedPositions);
  }
  if (shader) {
    shader = shader->makeWithPreLocalMatrix(matrix);
  } else {
    shader = tgfx::Shader::MakeColorShader(colors.back());
  }
  return shader;
}
}  // namespace pag
