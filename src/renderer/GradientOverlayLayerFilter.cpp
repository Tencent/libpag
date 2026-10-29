/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  or required by applicable law, agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "renderer/GradientOverlayLayerFilter.h"
#include <cmath>
#include "base/utils/MathUtil.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/Paint.h"
#include "tgfx/core/PictureRecorder.h"
#include "tgfx/core/Shader.h"

namespace pagx {

void GradientOverlayLayerFilter::setAlpha(float value) {
  setGradientOverlayField(_alpha, value);
}

void GradientOverlayLayerFilter::setAngle(float value) {
  setGradientOverlayField(_angle, value);
}

void GradientOverlayLayerFilter::setOverlayType(GradientOverlayType value) {
  setGradientOverlayField(_overlayType, value);
}

void GradientOverlayLayerFilter::setReverse(bool value) {
  setGradientOverlayField(_reverse, value);
}

void GradientOverlayLayerFilter::setScale(float value) {
  setGradientOverlayField(_scale, value);
}

void GradientOverlayLayerFilter::setOffsetX(float value) {
  setGradientOverlayField(_offsetX, value);
}

void GradientOverlayLayerFilter::setOffsetY(float value) {
  setGradientOverlayField(_offsetY, value);
}

void GradientOverlayLayerFilter::setBlendMode(tgfx::BlendMode value) {
  if (_blendMode == value) {
    return;
  }
  _blendMode = value;
  invalidateFilter();
}

void GradientOverlayLayerFilter::setGradientStops(std::vector<tgfx::Color> colors,
                                                  std::vector<float> positions) {
  _colors = std::move(colors);
  _positions = std::move(positions);
  invalidateFilter();
}

std::shared_ptr<tgfx::Image> GradientOverlayLayerFilter::onFilterImage(
    std::shared_ptr<tgfx::Image> input, float, const tgfx::Rect& contentBounds, tgfx::Point*,
    const tgfx::Point& inputOrigin) {
  if (input == nullptr || _colors.empty() || _positions.empty()) {
    return input;
  }
  // The shader math below mirrors the legacy GradientOverlayFilter::draw() verbatim, evaluated
  // in the CONTENT's coordinate space: contentBounds' dimensions equal the legacy source
  // raster's, so the angle-sensitive geometry (the conic sweep above all) keeps the legacy's
  // exact aspect no matter what scale the framework rasterizes the layer at.
  auto width = contentBounds.width();
  auto height = contentBounds.height();
  if (width <= 0.0f || height <= 0.0f) {
    width = static_cast<float>(input->width());
    height = static_cast<float>(input->height());
  }

  auto angle = fmodf(_angle, 360.0f);
  auto gradientScale = _scale / 100.0f;
  auto diagonalAngle = pag::RadiansToDegrees(std::atan(height / width));
  if (angle < diagonalAngle) {
    gradientScale *= 1.0f / std::cos(pag::DegreesToRadians(angle));
  } else if (angle < 180.0f - diagonalAngle) {
    gradientScale *= height / (width * std::cos(pag::DegreesToRadians(std::abs(90.0f - angle))));
  } else if (angle < 180.0f + diagonalAngle) {
    gradientScale *= 1.0f / std::cos(pag::DegreesToRadians(std::abs(180.0f - angle)));
  } else if (angle < 360.0f - diagonalAngle) {
    gradientScale *= height / (width * std::cos(pag::DegreesToRadians(std::abs(270.0f - angle))));
  } else if (angle < 360.0f) {
    gradientScale *= 1.0f / std::cos(pag::DegreesToRadians(360.0f - angle));
  }

  auto centerX = width * 0.5f;
  auto centerY = height * 0.5f;
  auto startPoint = tgfx::Point::Make(0.0f, centerY);
  auto endPoint = tgfx::Point::Make(width, centerY);

  auto matrix = tgfx::Matrix::I();
  matrix.postScale(gradientScale, gradientScale, centerX, centerY);
  matrix.postRotate(360.0f - angle, centerX, centerY);
  matrix.postTranslate(_offsetX * 0.01f * width, _offsetY * 0.01f * height);
  // Map content units onto the input pixels: pixel = k * (contentPoint + contentBounds.origin -
  // inputOrigin). The per-axis ratios reproduce the input raster's placement of the content,
  // which is what the framework draws the filtered output against.
  auto scaleX = static_cast<float>(input->width()) / width;
  auto scaleY = static_cast<float>(input->height()) / height;
  matrix.postScale(scaleX, scaleY);
  matrix.postTranslate(scaleX * (contentBounds.left - inputOrigin.x),
                       scaleY * (contentBounds.top - inputOrigin.y));

  auto colors = _colors;
  auto positions = _positions;
  if (_reverse) {
    std::swap(startPoint, endPoint);
    std::reverse(colors.begin(), colors.end());
    std::reverse(positions.begin(), positions.end());
    for (auto& position : positions) {
      position = 1.0f - position;
    }
  }

  // The four geometry branches mirror the legacy GradientPaint::getShader(): reflected doubles
  // the stop list around the 0.5 midpoint into one linear ramp, and angle sweeps a conic around
  // the start/end midpoint.
  std::shared_ptr<tgfx::Shader> shader = nullptr;
  switch (_overlayType) {
    case GradientOverlayType::Radial: {
      auto radius = tgfx::Point::Distance(startPoint, endPoint);
      shader = tgfx::Shader::MakeRadialGradient(startPoint, radius, colors, positions);
      break;
    }
    case GradientOverlayType::Angle: {
      auto center = startPoint + endPoint;
      center.set(center.x * 0.5f, center.y * 0.5f);
      shader = tgfx::Shader::MakeConicGradient(center, 0, 360, colors, positions);
      break;
    }
    case GradientOverlayType::Reflected: {
      auto reflectedColors = colors;
      reflectedColors.insert(reflectedColors.begin(), colors.rbegin(), colors.rend() - 1);
      auto reflectedPositions = positions;
      reflectedPositions.insert(reflectedPositions.begin(), positions.rbegin(),
                                positions.rend() - 1);
      auto size = static_cast<int>(positions.size());
      for (int i = 0; i < size; i++) {
        reflectedPositions[i] = (1.0f - reflectedPositions[i]) * 0.5f;
      }
      for (size_t i = static_cast<size_t>(size); i < reflectedPositions.size(); i++) {
        reflectedPositions[i] = reflectedPositions[i] * 0.5f + 0.5f;
      }
      shader = tgfx::Shader::MakeLinearGradient(startPoint, endPoint, reflectedColors,
                                                reflectedPositions);
      break;
    }
    case GradientOverlayType::Linear:
      shader = tgfx::Shader::MakeLinearGradient(startPoint, endPoint, colors, positions);
      break;
  }
  if (shader == nullptr) {
    shader = tgfx::Shader::MakeColorShader(colors.back());
  }
  shader = shader->makeWithMatrix(matrix);

  auto opacity = static_cast<uint8_t>(_alpha * 255.0f + 0.5f);
  if (opacity != 255) {
    shader = tgfx::Shader::MakeBlend(
        tgfx::BlendMode::DstIn, shader,
        tgfx::Shader::MakeColorShader(tgfx::Color::FromRGBA(0, 0, 0, opacity)));
  }
  auto imageShader = tgfx::Shader::MakeImageShader(input);
  // Mask the gradient to the content's alpha, then composite it over the content with the
  // style's blend mode (the content is the destination, matching the legacy shader order).
  shader = tgfx::Shader::MakeBlend(tgfx::BlendMode::DstIn, shader, imageShader);
  shader = tgfx::Shader::MakeBlend(_blendMode, imageShader, shader);

  tgfx::PictureRecorder recorder = {};
  auto* canvas = recorder.beginRecording();
  tgfx::Paint paint = {};
  paint.setShader(std::move(shader));
  canvas->drawRect(tgfx::Rect::MakeWH(width, height), paint);
  auto picture = recorder.finishRecordingAsPicture();
  if (picture == nullptr) {
    return input;
  }
  return tgfx::Image::MakeFrom(std::move(picture), input->width(), input->height());
}

}  // namespace pagx
