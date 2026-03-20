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

namespace pag {

PAGRenderer::PAGRenderer(PAGViewModel* viewModel, ContentView* contentView)
    : viewModel(viewModel), contentView(contentView) {
}

bool PAGRenderer::isReady() const {
  return viewModel != nullptr && viewModel->getPAGPlayer() != nullptr &&
         viewModel->getPAGFile() != nullptr;
}

void PAGRenderer::updateSize() {
  auto pagSurface = viewModel->getPAGPlayer()->getSurface();
  if (pagSurface != nullptr) {
    pagSurface->updateSize();
  }
}

IContentRenderer::RenderMetrics PAGRenderer::flush() {
  RenderMetrics metrics = {};
  if (!isReady()) {
    return metrics;
  }
  if (contentView->takeSizeChanged()) {
    updateSize();
  }
  auto* player = viewModel->getPAGPlayer();
  auto* file = viewModel->getPAGFile();
  player->flush();
  double progress = file->getProgress();
  auto totalFrames =
      static_cast<int64_t>(std::round(player->duration() * file->frameRate() / 1000000.0));
  metrics.currentFrame =
      static_cast<int64_t>(std::round(static_cast<double>(totalFrames - 1) * progress));
  metrics.renderTime = player->renderingTime();
  metrics.presentTime = player->presentingTime();
  metrics.imageDecodeTime = player->imageDecodingTime();
  metrics.rendered = true;
  QMetaObject::invokeMethod(contentView, "update", Qt::QueuedConnection);
  return metrics;
}

}  // namespace pag
