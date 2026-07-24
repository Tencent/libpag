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

#include "rendering/pag/PAGRenderer.h"
#include <algorithm>

namespace pag {

PAGRenderer::PAGRenderer(PAGViewModel* viewModel) : viewModel(viewModel) {
}

void PAGRenderer::setDrawable(GPUDrawable* drawable) {
  this->drawable = drawable;
}

bool PAGRenderer::isReady() const {
  return viewModel != nullptr && viewModel->getPAGPlayer() != nullptr &&
         viewModel->getPAGPlayer()->getComposition() != nullptr;
}

void PAGRenderer::updateSize() {
  auto pagSurface = viewModel->getPAGPlayer()->getSurface();
  if (pagSurface != nullptr) {
    pagSurface->updateSize();
  }
}

void PAGRenderer::applyDisplayTransform() {
  if (drawable == nullptr || drawable->width() <= 0 || drawable->height() <= 0 ||
      viewModel->getWidth() <= 0 || viewModel->getHeight() <= 0) {
    return;
  }
  auto surfaceWidth = static_cast<float>(drawable->width());
  auto surfaceHeight = static_cast<float>(drawable->height());
  auto contentWidth = static_cast<float>(viewModel->getWidth());
  auto contentHeight = static_cast<float>(viewModel->getHeight());
  auto contentScale = std::min(surfaceWidth / contentWidth, surfaceHeight / contentHeight);
  auto contentOffsetX = (surfaceWidth - contentWidth * contentScale) * 0.5f;
  auto contentOffsetY = (surfaceHeight - contentHeight * contentScale) * 0.5f;
  auto transform = viewModel->getViewTransform();
  auto zoomScale = static_cast<float>(transform.zoomScale);
  auto matrix = Matrix::MakeAll(contentScale * zoomScale, 0.0f,
                                contentOffsetX * zoomScale + static_cast<float>(transform.offsetX),
                                0.0f, contentScale * zoomScale,
                                contentOffsetY * zoomScale + static_cast<float>(transform.offsetY),
                                0.0f, 0.0f, 1.0f);
  viewModel->getPAGPlayer()->setMatrix(matrix);
}

IContentRenderer::RenderMetrics PAGRenderer::flush() {
  RenderMetrics metrics = {};
  if (!isReady()) {
    return metrics;
  }
  auto* player = viewModel->getPAGPlayer();
  auto* file = viewModel->getPAGFile();
  if (file == nullptr) {
    return metrics;
  }
  applyDisplayTransform();
  player->flush();
  double progress = file->getProgress();
  auto totalFrames =
      static_cast<int64_t>(std::round(player->duration() * file->frameRate() / 1000000.0));
  if (totalFrames < 1) {
    metrics.currentFrame = 0;
  } else {
    metrics.currentFrame =
        static_cast<int64_t>(std::round(static_cast<double>(totalFrames - 1) * progress));
  }
  metrics.renderTime = player->renderingTime();
  metrics.presentTime = player->presentingTime();
  metrics.imageDecodeTime = player->imageDecodingTime();
  metrics.rendered = true;
  return metrics;
}

}  // namespace pag
