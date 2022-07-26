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

#include "GradientOverlayFilter.h"
#include "base/utils/MathUtil.h"
#include "base/utils/TGFXCast.h"
#include "rendering/graphics/GradientPaint.h"
#include "tgfx/gpu/Surface.h"
#include "tgfx/gpu/opengl/GLRenderTarget.h"

namespace pag {
GradientOverlayFilter::GradientOverlayFilter(GradientOverlayStyle* layerStyle)
    : layerStyle(layerStyle) {
}

bool GradientOverlayFilter::initialize(tgfx::Context*) {
  return true;
}

void GradientOverlayFilter::draw(tgfx::Context* context, const FilterSource* source,
                                 const FilterTarget* target) {
  auto* gradStyle = static_cast<GradientOverlayStyle*>(layerStyle);
  auto opacity = gradStyle->opacity->getValueAt(layerFrame);
  auto colors = gradStyle->colors->getValueAt(layerFrame);
  auto angle = gradStyle->angle->getValueAt(layerFrame);
  auto style = gradStyle->style->getValueAt(layerFrame);
  auto reverse = gradStyle->reverse->getValueAt(layerFrame);
  auto scale = gradStyle->scale->getValueAt(layerFrame) / 100.f;
  auto offset = gradStyle->offset->getValueAt(layerFrame);

  auto width = static_cast<float>(source->width);
  auto height = static_cast<float>(source->height);

  angle = fmodf(angle, 360.f);
  auto diagonalAngle = RadiansToDegrees(std::atan(height / width));
  if (angle < diagonalAngle) {
    scale *= 1.f / std::cos(DegreesToRadians(angle));
  } else if (angle < 180.f - diagonalAngle) {
    scale *= height / (width * std::cos(DegreesToRadians(std::abs(90.f - angle))));
  } else if (angle < 180.f + diagonalAngle) {
    scale *= 1.f / std::cos(DegreesToRadians(std::abs(180.f - angle)));
  } else if (angle < 360.f - diagonalAngle) {
    scale *= height / (width * std::cos(DegreesToRadians(std::abs(270.f - angle))));
  } else if (angle < 360.f) {
    scale *= 1.f / std::cos(DegreesToRadians(360.f - angle));
  }

  auto centerX = width * 0.5f;
  auto centerY = height * 0.5f;
  auto startPoint = Point::Make(0.f, centerY);
  auto endPoint = Point::Make(width, centerY);

  auto matrix = tgfx::Matrix::I();
  matrix.postScale(scale, scale, centerX, centerY);
  matrix.postRotate(360.f - angle, centerX, centerY);
  matrix.postTranslate(offset.x * 0.01f * width, offset.y * 0.01f * height);

  auto gradient = GradientPaint(style, startPoint, endPoint, colors, matrix, reverse);
  auto shader = gradient.getShader();

  if (opacity != 255) {
    shader = tgfx::Shader::MakeBlend(
        tgfx::BlendMode::DstIn, shader,
        tgfx::Shader::MakeColorShader(tgfx::Color::FromRGBA(0, 0, 0, opacity)));
  }

  auto renderTarget = tgfx::GLRenderTarget::MakeFrom(context, target->frameBuffer, target->width,
                                                     target->height, tgfx::ImageOrigin::TopLeft);
  auto targetSurface = tgfx::Surface::MakeFrom(renderTarget);
  auto targetCanvas = targetSurface->getCanvas();
  auto texture = tgfx::GLTexture::MakeFrom(context, source->sampler, source->width, source->height,
                                           tgfx::ImageOrigin::TopLeft);
  shader = tgfx::Shader::MakeBlend(ToTGFXBlend(gradStyle->blendMode->getValueAt(layerFrame)),
                                   tgfx::Shader::MakeTextureShader(texture), shader);
  tgfx::Paint paint;
  paint.setShader(shader);
  targetCanvas->save();
  targetCanvas->setMatrix(ToMatrix(target));
  targetCanvas->concat(tgfx::Matrix::MakeScale(1.f / filterScale.x, 1.f / filterScale.y));
  auto rect = tgfx::Rect::MakeWH(width, height);
  targetCanvas->drawRect(rect, paint);
  targetCanvas->restore();
}
}  // namespace pag
