/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "PAGRenderThread.h"
#include <QGuiApplication>
#include "rendering/PAGView.h"

namespace pag {

PAGRenderThread::PAGRenderThread(PAGView* pagView) : pagView(pagView) {
}

void PAGRenderThread::flush() {
  if (pagView->pagPlayer == nullptr || pagView->pagFile == nullptr) {
    return;
  }
  if (pagView->sizeChanged) {
    pagView->sizeChanged = false;
    auto pagSurface = pagView->pagPlayer->getSurface();
    pagSurface->updateSize();
  }
  pagView->pagPlayer->flush();
  double progress = pagView->pagFile->getProgress();
  int64_t currentFrame =
      static_cast<int64_t>(std::round((pagView->getTotalFrame().toDouble() - 1) * progress));
  int64_t renderingTime = pagView->pagPlayer->renderingTime();
  int64_t presentingTime = pagView->pagPlayer->presentingTime();
  int64_t imageDecodingTime = pagView->pagPlayer->imageDecodingTime();
  Q_EMIT frameTimeMetricsReady(currentFrame, renderingTime, presentingTime, imageDecodingTime);
  QMetaObject::invokeMethod(pagView, "update", Qt::QueuedConnection);
}

void PAGRenderThread::shutDown() {
  if (QGuiApplication::instance()) {
    auto mainThread = QGuiApplication::instance()->thread();
    if (pagView->drawable) {
      pagView->drawable->moveToThread(mainThread);
    }
    moveToThread(mainThread);
  }
  exit();
}

}  // namespace pag