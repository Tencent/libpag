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

#include "RenderThread.h"
#include <QGuiApplication>
#include "ContentView.h"

namespace pag {

RenderThread::RenderThread(ContentView* view, std::unique_ptr<IContentRenderer> renderer)
    : view(view), renderer(std::move(renderer)) {
}

void RenderThread::flush() {
  if (renderer == nullptr) {
    return;
  }
  if (view->takeSizeChanged()) {
    renderer->updateSize();
  }
  auto metrics = renderer->flush();
  if (!metrics.rendered) {
    return;
  }
  Q_EMIT renderMetricsReady(metrics.renderTime, metrics.presentTime, metrics.imageDecodeTime,
                            metrics.currentFrame);
}

void RenderThread::shutDown() {
  if (view != nullptr && QGuiApplication::instance() != nullptr) {
    auto mainThread = QGuiApplication::instance()->thread();
    if (view->drawable != nullptr) {
      view->drawable->moveToThread(mainThread);
    }
  }
  exit();
}

}  // namespace pag
