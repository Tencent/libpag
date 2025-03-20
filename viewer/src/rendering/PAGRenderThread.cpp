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

#include "PAGRenderThread.h"
#include <QGuiApplication>

namespace pag {

PAGRenderThread::PAGRenderThread(PAGView* pagView) : pagView(pagView) {
}

auto PAGRenderThread::flush() -> void {
  pagView->pagPlayer->flush();
  QMetaObject::invokeMethod(pagView, "update", Qt::QueuedConnection);
}

auto PAGRenderThread::shutDown() -> void {
  exit();
  if (QGuiApplication::instance()) {
    auto mainThread = QGuiApplication::instance()->thread();
    if (pagView->drawable) {
      pagView->drawable->moveToThread(mainThread);
    }
    moveToThread(mainThread);
  }
}

}  // namespace pag