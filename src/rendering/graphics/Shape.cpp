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

#include "Shape.h"
#include "pag/file.h"
#include "rendering/caches/RenderCache.h"
#include "tgfx/core/Mask.h"
#include "tgfx/core/Shape.h"
#include "tgfx/gpu/Canvas.h"
#include "tgfx/gpu/Shader.h"
#include "tgfx/gpu/Surface.h"

namespace pag {
std::shared_ptr<Graphic> Shape::MakeFrom(ID assetID, const tgfx::Path& path, tgfx::Color color) {
  if (path.isEmpty()) {
    return nullptr;
  }
  auto shader = tgfx::Shader::MakeColorShader(color);
  return std::shared_ptr<Graphic>(new Shape(assetID, path, std::move(shader)));
}

std::shared_ptr<Graphic> Shape::MakeFrom(ID assetID, const tgfx::Path& path,
                                         const GradientPaint& gradient) {
  if (path.isEmpty()) {
    return nullptr;
  }
  return std::shared_ptr<Graphic>(new Shape(assetID, path, gradient.getShader()));
}

Shape::Shape(ID assetID, tgfx::Path path, std::shared_ptr<tgfx::Shader> shader)
    : assetID(assetID), path(std::move(path)), shader(std::move(shader)) {
}

void Shape::measureBounds(tgfx::Rect* bounds) const {
  *bounds = path.getBounds();
}

bool Shape::hitTest(RenderCache*, float x, float y) {
  return path.contains(x, y);
}

bool Shape::getPath(tgfx::Path* result) const {
  if (shader && !shader->isOpaque()) {
    return false;
  }
  result->addPath(path);
  return true;
}

void Shape::prepare(RenderCache*) const {
}

void Shape::draw(tgfx::Canvas* canvas, RenderCache* renderCache) const {
  tgfx::Paint paint;
  auto shape = renderCache->getShape(assetID, path);
  if (shape == nullptr) {
    paint.setShader(shader);
    canvas->drawPath(path, paint);
    return;
  }
  auto matrix = tgfx::Matrix::MakeScale(shape->resolutionScale());
  paint.setShader(shader->makeWithPostLocalMatrix(matrix));
  auto oldMatrix = canvas->getMatrix();
  canvas->concat(tgfx::Matrix::MakeScale(1 / shape->resolutionScale()));
  canvas->drawShape(shape, paint);
  canvas->setMatrix(oldMatrix);
}
}  // namespace pag
