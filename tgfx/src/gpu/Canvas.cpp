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

#include "gpu/Canvas.h"
#include "TextureMaskFragmentProcessor.h"
#include "core/Mask.h"
#include "gpu/Surface.h"

namespace tgfx {
Canvas::Canvas(Surface* surface) : surface(surface) {
  globalPaint.clip.addRect(0, 0, static_cast<float>(surface->width()),
                           static_cast<float>(surface->height()));
}

void Canvas::save() {
  onSave();
  savedPaintList.push_back(globalPaint);
}

void Canvas::restore() {
  if (savedPaintList.empty()) {
    return;
  }
  globalPaint = savedPaintList.back();
  savedPaintList.pop_back();
  onRestore();
}

Matrix Canvas::getMatrix() const {
  return globalPaint.matrix;
}

void Canvas::setMatrix(const Matrix& matrix) {
  globalPaint.matrix = matrix;
  onSetMatrix(globalPaint.matrix);
}

void Canvas::resetMatrix() {
  globalPaint.matrix.reset();
  onSetMatrix(globalPaint.matrix);
}

void Canvas::concat(const Matrix& matrix) {
  globalPaint.matrix.preConcat(matrix);
  onSetMatrix(globalPaint.matrix);
}

float Canvas::getAlpha() const {
  return globalPaint.alpha;
}

void Canvas::setAlpha(float newAlpha) {
  globalPaint.alpha = newAlpha;
}

BlendMode Canvas::getBlendMode() const {
  return globalPaint.blendMode;
}

void Canvas::setBlendMode(BlendMode blendMode) {
  globalPaint.blendMode = blendMode;
}

Path Canvas::getTotalClip() const {
  return globalPaint.clip;
}

void Canvas::clipPath(const Path& path) {
  onClipPath(path);
  auto clipPath = path;
  clipPath.transform(globalPaint.matrix);
  globalPaint.clip.addPath(clipPath, PathOp::Intersect);
  globalPaint.clipTexture = nullptr;
}

void Canvas::drawRect(const Rect& rect, const Paint& paint) {
  Path path = {};
  path.addRect(rect);
  drawPath(path, paint);
}

void Canvas::drawTexture(const Texture* texture) {
  drawTexture(texture, nullptr);
}

void Canvas::drawTexture(const Texture* texture, const Matrix& matrix) {
  auto oldMatrix = getMatrix();
  concat(matrix);
  drawTexture(texture, nullptr);
  setMatrix(oldMatrix);
}

Context* Canvas::getContext() const {
  return surface->getContext();
}

Surface* Canvas::getSurface() const {
  return surface;
}

const SurfaceOptions* Canvas::surfaceOptions() const {
  return surface->options();
}

std::unique_ptr<FragmentProcessor> Canvas::getClipFragmentProcessor() {
  if (!globalPaint.clip.isEmpty() && globalPaint.clipTexture == nullptr) {
    auto bounds = globalPaint.clip.getBounds();
    auto width = ceilf(bounds.width());
    auto height = ceilf(bounds.height());
    if (width == 0 || height == 0) {
      return nullptr;
    }
    auto mask = Mask::Make(static_cast<int>(width), static_cast<int>(height));
    auto matrix = Matrix::MakeTrans(-bounds.x(), -bounds.y());
    matrix.postScale(width / bounds.width(), height / bounds.height());
    mask->setMatrix(matrix);
    mask->fillPath(globalPaint.clip);
    globalPaint.clipTexture = mask->makeTexture(getContext());
    auto textureMatrix = Matrix::I();
    if (surface->origin() == ImageOrigin::BottomLeft) {
      textureMatrix.postScale(1, -1);
      textureMatrix.postTranslate(0, static_cast<float>(surface->height()));
    }
    textureMatrix.postTranslate(-bounds.x(), -bounds.y());
    textureMatrix.postScale(1.f / bounds.width(), 1.f / bounds.height());
    globalPaint.clipTextureMatrix = textureMatrix;
  }
  return TextureMaskFragmentProcessor::MakeUseDeviceCoord(globalPaint.clipTexture.get(),
                                                          globalPaint.clipTextureMatrix);
}
}  // namespace tgfx
