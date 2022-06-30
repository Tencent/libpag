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
  auto snapshot = renderCache->getSnapshot(this);
  if (snapshot) {
    auto matrix = snapshot->getMatrix();
    if (!matrix.invert(&matrix)) {
      return;
    }
    paint.setShader(shader->makeWithPostLocalMatrix(matrix));
    auto oldMatrix = canvas->getMatrix();
    canvas->concat(snapshot->getMatrix());
    auto texture = snapshot->getTexture();
    if (texture) {
      paint.setMaskFilter(tgfx::MaskFilter::Make(tgfx::Shader::MakeTextureShader(texture)));
      auto rect = tgfx::Rect::MakeWH(static_cast<float>(texture->width()),
                                     static_cast<float>(texture->height()));
      canvas->drawRect(rect, paint);
    } else if (snapshot->getMesh()) {
      canvas->drawMesh(snapshot->getMesh(), paint);
    }
    canvas->setMatrix(oldMatrix);
    return;
  }
  paint.setShader(shader);
  canvas->drawPath(path, paint);
}

std::unique_ptr<Snapshot> MakeMeshSnapshot(tgfx::Path path, RenderCache*, float scaleFactor) {
  auto matrix = tgfx::Matrix::MakeScale(scaleFactor);
  path.transform(matrix);
  auto mesh = tgfx::Mesh::MakeFrom(path);
  if (mesh == nullptr) {
    return nullptr;
  }
  auto drawingMatrix = tgfx::Matrix::I();
  matrix.invert(&drawingMatrix);
  return std::make_unique<Snapshot>(std::move(mesh), drawingMatrix);
}

std::unique_ptr<Snapshot> MakeTextureSnapshot(const tgfx::Path& path, RenderCache* cache,
                                              float scaleFactor) {
  auto bounds = path.getBounds();
  auto width = static_cast<int>(ceilf(bounds.width() * scaleFactor));
  auto height = static_cast<int>(ceilf(bounds.height() * scaleFactor));
  auto mask = tgfx::Mask::Make(width, height);
  if (mask == nullptr) {
    return nullptr;
  }
  auto matrix = tgfx::Matrix::MakeScale(scaleFactor);
  matrix.preTranslate(-bounds.x(), -bounds.y());
  mask->setMatrix(matrix);
  mask->fillPath(path);
  auto drawingMatrix = tgfx::Matrix::I();
  matrix.invert(&drawingMatrix);
  auto texture = mask->makeTexture(cache->getContext());
  if (texture == nullptr) {
    return nullptr;
  }
  return std::make_unique<Snapshot>(texture, drawingMatrix);
}

std::unique_ptr<Snapshot> Shape::makeSnapshot(RenderCache* cache, float scaleFactor) const {
  if (path.isEmpty() || path.getBounds().isEmpty()) {
    return nullptr;
  }
  auto bounds = path.getBounds();
  auto width = static_cast<int>(ceilf(bounds.width() * scaleFactor));
  auto height = static_cast<int>(ceilf(bounds.height() * scaleFactor));
  static constexpr int MaxSize = 50;
  if (path.asRect(nullptr) || path.asRRect(nullptr) || (std::max(width, height) > MaxSize)) {
    auto snapshot = MakeMeshSnapshot(path, cache, scaleFactor);
    if (snapshot) {
      return snapshot;
    }
  }
  return MakeTextureSnapshot(path, cache, scaleFactor);
}
}  // namespace pag
