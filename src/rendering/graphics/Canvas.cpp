/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "rendering/caches/RenderCache.h"
#include "tgfx/gpu/Surface.h"

namespace pag {
struct CanvasState {
  float alpha = 1.0f;
  tgfx::BlendMode blendMode = tgfx::BlendMode::SrcOver;
};

Canvas::Canvas(tgfx::Surface* surface, RenderCache* cache)
    : canvas(surface->getCanvas()), cache(cache) {
  state = std::make_shared<CanvasState>();
}

Canvas::Canvas(tgfx::Canvas* canvas, RenderCache* cache) : canvas(canvas), cache(cache) {
  state = std::make_shared<CanvasState>();
}

tgfx::Context* Canvas::getContext() const {
  return cache->getContext();
}

tgfx::Surface* Canvas::getSurface() const {
  return canvas->getSurface();
}

const tgfx::SurfaceOptions* Canvas::surfaceOptions() const {
  auto surface = canvas->getSurface();
  return surface ? surface->options() : nullptr;
}

void Canvas::save() {
  auto canvasState = std::make_shared<CanvasState>();
  *canvasState = *state;
  savedStateList.push_back(canvasState);
  canvas->save();
}

void Canvas::restore() {
  if (savedStateList.empty()) {
    return;
  }
  canvas->restore();
  state = savedStateList.back();
  savedStateList.pop_back();
}

float Canvas::getAlpha() const {
  return state->alpha;
}

void Canvas::setAlpha(float value) {
  state->alpha = value;
}

tgfx::BlendMode Canvas::getBlendMode() const {
  return state->blendMode;
}

void Canvas::setBlendMode(tgfx::BlendMode value) {
  state->blendMode = value;
}

const tgfx::Paint Canvas::createPaint(const tgfx::Paint* paint) const {
  tgfx::Paint result = {};
  if (paint != nullptr) {
    result = *paint;
  }
  result.setAlpha(result.getAlpha() * state->alpha);
  result.setBlendMode(state->blendMode);
  return result;
}
}  // namespace pag