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

#include "tgfx/gpu/Canvas.h"
#include <atomic>
#include "CanvasState.h"
#include "tgfx/gpu/Surface.h"

namespace tgfx {
static uint32_t NextClipID() {
  static const uint32_t kFirstUnreservedClipID = 1;
  static std::atomic_uint32_t nextID{kFirstUnreservedClipID};
  uint32_t id;
  do {
    id = nextID++;
  } while (id < kFirstUnreservedClipID);
  return id;
}

Canvas::Canvas(Surface* surface) : surface(surface) {
  state = std::make_shared<CanvasState>();
  state->clip.addRect(0, 0, static_cast<float>(surface->width()),
                      static_cast<float>(surface->height()));
  state->clipID = NextClipID();
}

void Canvas::save() {
  onSave();
  auto canvasState = std::make_shared<CanvasState>();
  *canvasState = *state;
  savedStateList.push_back(canvasState);
}

void Canvas::restore() {
  if (savedStateList.empty()) {
    return;
  }
  state = savedStateList.back();
  savedStateList.pop_back();
  onRestore();
}

Matrix Canvas::getMatrix() const {
  return state->matrix;
}

void Canvas::setMatrix(const Matrix& matrix) {
  state->matrix = matrix;
  onSetMatrix(state->matrix);
}

void Canvas::resetMatrix() {
  state->matrix.reset();
  onSetMatrix(state->matrix);
}

void Canvas::concat(const Matrix& matrix) {
  state->matrix.preConcat(matrix);
  onSetMatrix(state->matrix);
}

float Canvas::getAlpha() const {
  return state->alpha;
}

void Canvas::setAlpha(float newAlpha) {
  state->alpha = newAlpha;
}

BlendMode Canvas::getBlendMode() const {
  return state->blendMode;
}

void Canvas::setBlendMode(BlendMode blendMode) {
  state->blendMode = blendMode;
}

Path Canvas::getTotalClip() const {
  return state->clip;
}

void Canvas::clipPath(const Path& path) {
  onClipPath(path);
  auto clipPath = path;
  clipPath.transform(state->matrix);
  state->clip.addPath(clipPath, PathOp::Intersect);
  state->clipID = NextClipID();
}

void Canvas::drawRect(const Rect& rect, const Paint& paint) {
  Path path = {};
  path.addRect(rect);
  drawPath(path, paint);
}

void Canvas::drawTexture(const Texture* texture, const Matrix& matrix) {
  auto oldMatrix = getMatrix();
  concat(matrix);
  drawTexture(texture);
  setMatrix(oldMatrix);
}

void Canvas::drawTexture(const Texture* texture, const Paint* paint) {
  drawTexture(texture, nullptr, paint);
}

static Paint CleanPaintForDrawTexture(const Paint* paint) {
  Paint cleaned;
  if (paint) {
    cleaned = *paint;
    cleaned.setStyle(PaintStyle::Fill);
  }
  return cleaned;
}

void Canvas::drawTexture(const Texture* texture, const RGBAAALayout* layout, const Paint* paint) {
  auto realPaint = CleanPaintForDrawTexture(paint);
  std::shared_ptr<Texture> result;
  auto oldMatrix = getMatrix();
  if (realPaint.getImageFilter()) {
    auto clipBounds = state->clip.getBounds();
    Matrix inverted = Matrix::I();
    if (!oldMatrix.invert(&inverted)) {
      return;
    }
    inverted.mapRect(&clipBounds);
    ImageFilterContext context(getContext(), oldMatrix, clipBounds, texture);
    auto [image, offset] = realPaint.getImageFilter()->filterImage(context);
    result = image;
    texture = result.get();
    concat(Matrix::MakeTrans(offset.x, offset.y));
  }
  drawTexture(texture, layout, realPaint);
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
}  // namespace tgfx
