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
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "rendering/pagx/PAGXRenderer.h"
#include "tgfx/core/Clock.h"

namespace pag {

PAGXRenderer::PAGXRenderer(PAGXViewModel* viewModel) : viewModel(viewModel) {
}

void PAGXRenderer::setDrawable(GPUDrawable* drawable) {
  this->drawable = drawable;
}

bool PAGXRenderer::isReady() const {
  return viewModel != nullptr && drawable != nullptr;
}

void PAGXRenderer::updateSize() {
  if (drawable != nullptr) {
    drawable->freeSurface();
    drawable->updateSize();
  }
}

IContentRenderer::RenderMetrics PAGXRenderer::flush() {
  RenderMetrics metrics = {};
  if (!viewModel->takeNeedsRender()) {
    return metrics;
  }
  if (!isReady()) {
    return metrics;
  }

  auto state = viewModel->getRenderState();
  if (state.displayList == nullptr) {
    return metrics;
  }

  auto device = this->drawable->getDevice();
  if (device == nullptr) {
    return metrics;
  }
  auto context = device->lockContext();
  if (context == nullptr) {
    return metrics;
  }
  auto surface = this->drawable->getSurface(context, false);
  if (surface == nullptr) {
    device->unlock();
    return metrics;
  }

  if (state.rootLayer != nullptr && state.contentWidth > 0 && state.contentHeight > 0) {
    int surfaceWidth = surface->width();
    int surfaceHeight = surface->height();
    float scaleX = static_cast<float>(surfaceWidth) / static_cast<float>(state.contentWidth);
    float scaleY = static_cast<float>(surfaceHeight) / static_cast<float>(state.contentHeight);
    float scale = std::min(scaleX, scaleY);
    float offsetX =
        (static_cast<float>(surfaceWidth) - static_cast<float>(state.contentWidth) * scale) * 0.5f;
    float offsetY =
        (static_cast<float>(surfaceHeight) - static_cast<float>(state.contentHeight) * scale) *
        0.5f;
    auto matrix = tgfx::Matrix::MakeTrans(offsetX, offsetY);
    matrix.preScale(scale, scale);
    state.rootLayer->setMatrix(matrix);
  }

  auto renderStart = tgfx::Clock::Now();
  auto canvas = surface->getCanvas();
  canvas->clear();
  state.displayList->render(surface.get());
  auto recording = context->flush();
  metrics.renderTime = tgfx::Clock::Now() - renderStart;

  auto imageStart = tgfx::Clock::Now();
  if (recording) {
    context->submit(std::move(recording));
  }
  metrics.imageDecodeTime = tgfx::Clock::Now() - imageStart;

  auto presentStart = tgfx::Clock::Now();
  this->drawable->present(context);
  metrics.presentTime = tgfx::Clock::Now() - presentStart;

  metrics.rendered = true;
  device->unlock();
  return metrics;
}

}  // namespace pag
