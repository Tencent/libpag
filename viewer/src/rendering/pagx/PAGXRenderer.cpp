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

PAGXRenderer::PAGXRenderer(PAGXViewModel* viewModel, ContentView* contentView)
    : viewModel(viewModel), contentView(contentView) {
}

bool PAGXRenderer::isReady() const {
  return viewModel != nullptr && viewModel->getDisplayList() != nullptr &&
         contentView->getDrawable() != nullptr;
}

void PAGXRenderer::updateSize() {
  contentView->getDrawable()->updateSize();
}

IContentRenderer::RenderMetrics PAGXRenderer::flush() {
  RenderMetrics metrics = {};
  if (contentView->takeSizeChanged()) {
    updateSize();
  }
  if (!viewModel->takeNeedsRender()) {
    return metrics;
  }
  if (!isReady()) {
    return metrics;
  }

  auto* drawable = contentView->getDrawable();
  auto device = drawable->getDevice();
  if (device == nullptr) {
    return metrics;
  }
  auto context = device->lockContext();
  if (context == nullptr) {
    device->unlock();
    return metrics;
  }
  auto surface = drawable->getSurface(context, false);
  if (surface == nullptr) {
    device->unlock();
    return metrics;
  }

  auto* contentLayer = viewModel->getContentLayer();
  int contentWidth = viewModel->getContentWidth();
  int contentHeight = viewModel->getContentHeight();
  if (contentLayer != nullptr && contentWidth > 0 && contentHeight > 0) {
    int surfaceWidth = surface->width();
    int surfaceHeight = surface->height();
    float scaleX = static_cast<float>(surfaceWidth) / static_cast<float>(contentWidth);
    float scaleY = static_cast<float>(surfaceHeight) / static_cast<float>(contentHeight);
    float scale = std::min(scaleX, scaleY);
    float offsetX =
        (static_cast<float>(surfaceWidth) - static_cast<float>(contentWidth) * scale) * 0.5f;
    float offsetY =
        (static_cast<float>(surfaceHeight) - static_cast<float>(contentHeight) * scale) * 0.5f;
    auto matrix = tgfx::Matrix::MakeTrans(offsetX, offsetY);
    matrix.preScale(scale, scale);
    contentLayer->setMatrix(matrix);
  }

  auto renderStart = tgfx::Clock::Now();
  auto canvas = surface->getCanvas();
  canvas->clear();
  viewModel->getDisplayList()->render(surface.get());
  auto recording = context->flush();
  metrics.renderTime = tgfx::Clock::Now() - renderStart;

  auto imageStart = tgfx::Clock::Now();
  if (recording) {
    context->submit(std::move(recording));
  }
  metrics.imageDecodeTime = tgfx::Clock::Now() - imageStart;

  auto presentStart = tgfx::Clock::Now();
  drawable->present(context);
  metrics.presentTime = tgfx::Clock::Now() - presentStart;

  metrics.rendered = true;
  device->unlock();
  return metrics;
}

}  // namespace pag
