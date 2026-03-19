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

#include "PAGRenderer.h"
#include "PAGView.h"

namespace pag {

PAGRenderer::PAGRenderer(PAGView* view) : view(view) {
}

bool PAGRenderer::isReady() const {
  return view != nullptr && view->pagPlayer != nullptr && view->pagFile != nullptr;
}

void PAGRenderer::updateSize() {
  auto pagSurface = view->pagPlayer->getSurface();
  pagSurface->updateSize();
}

IContentRenderer::RenderMetrics PAGRenderer::flush() {
  RenderMetrics metrics = {};
  if (!isReady()) {
    return metrics;
  }
  if (view->sizeChanged) {
    view->sizeChanged = false;
    updateSize();
  }
  view->pagPlayer->flush();
  double progress = view->pagFile->getProgress();
  metrics.currentFrame =
      static_cast<int64_t>(std::round((view->getTotalFrame().toDouble() - 1) * progress));
  metrics.renderTime = view->pagPlayer->renderingTime();
  metrics.presentTime = view->pagPlayer->presentingTime();
  metrics.imageDecodeTime = view->pagPlayer->imageDecodingTime();
  QMetaObject::invokeMethod(view, "update", Qt::QueuedConnection);
  return metrics;
}

}  // namespace pag
