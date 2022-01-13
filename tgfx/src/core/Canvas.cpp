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

#include "Canvas.h"
#include "base/utils/MatrixUtil.h"
#include "gpu/Surface.h"

namespace pag {
#define CONTENT_SCALE_STEP 20.0f

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

void Canvas::concatAlpha(Opacity alpha) {
  globalPaint.alpha = OpacityConcat(globalPaint.alpha, alpha);
}

void Canvas::concatBlendMode(Blend blendMode) {
  if (blendMode == Blend::SrcOver) {
    return;
  }
  globalPaint.blendMode = blendMode;
}

Path Canvas::getGlobalClip() const {
  return globalPaint.clip;
}

void Canvas::clipPath(const Path& path) {
  onClipPath(path);
  auto clipPath = path;
  clipPath.transform(globalPaint.matrix);
  globalPaint.clip.addPath(clipPath, PathOp::Intersect);
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

const SurfaceOptions* Canvas::surfaceOptions() const {
  return surface->options();
}

std::shared_ptr<Surface> Canvas::makeContentSurface(const Rect& bounds, float scaleFactorLimit,
                                                    bool usesMSAA) const {
  auto totalMatrix = getMatrix();
  auto maxScale = GetMaxScaleFactor(totalMatrix);
  if (maxScale > scaleFactorLimit) {
    maxScale = scaleFactorLimit;
  } else {
    // Corrected the zoom value to snap to 1/20th the accuracy of pixels to prevent edge jitter
    // during gradual zoom-in.
    // 1/20 is the minimum pixel accuracy for most platforms, and there is no jitter when drawing
    // on the screen.
    maxScale = ceilf(maxScale * CONTENT_SCALE_STEP) / CONTENT_SCALE_STEP;
  }
  auto width = static_cast<int>(ceilf(bounds.width() * maxScale));
  auto height = static_cast<int>(ceil(bounds.height() * maxScale));
  // LOGE("makeContentSurface: (width = %d, height = %d)", width, height);
  auto sampleCount = usesMSAA ? 4 : 1;
  auto newSurface = Surface::Make(getContext(), width, height, false, sampleCount);
  if (newSurface == nullptr) {
    return nullptr;
  }
  auto newCanvas = newSurface->getCanvas();
  auto matrix = Matrix::MakeScale(maxScale);
  matrix.preTranslate(-bounds.x(), -bounds.y());
  newCanvas->setMatrix(matrix);
  return newSurface;
}
}  // namespace pag
