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

#include "PAGXRenderThread.h"
#include <QGuiApplication>
#include "PAGXView.h"

namespace pag {

PAGXRenderThread::PAGXRenderThread(PAGXView* view) : pagxView(view) {
}

void PAGXRenderThread::flush() {
  if (pagxView == nullptr) {
    return;
  }
  if (pagxView->sizeChanged) {
    pagxView->sizeChanged = false;
    pagxView->drawable->updateSize();
  }
  if (!pagxView->needsRender) {
    return;
  }
  pagxView->needsRender = false;

  auto metrics = pagxView->renderPAGX();

  Q_EMIT renderTimeReady(metrics.renderTime, metrics.imageTime, metrics.presentTime);
}

void PAGXRenderThread::shutDown() {
  if (QGuiApplication::instance()) {
    auto mainThread = QGuiApplication::instance()->thread();
    if (pagxView->drawable) {
      pagxView->drawable->moveToThread(mainThread);
    }
    moveToThread(mainThread);
  }
  exit();
}
}  // namespace pag
