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

#pragma once

#include <QThread>

namespace pag {

class PAGXView;

/**
 * PAGXRenderThread handles rendering of static PAGX content on a separate thread
 * to avoid blocking the main UI thread for large or complex PAGX files.
 */
class PAGXRenderThread : public QThread {
  Q_OBJECT
 public:
  explicit PAGXRenderThread(PAGXView* view);

  Q_SIGNAL void renderTimeReady(int64_t renderTime, int64_t imageTime, int64_t presentTime);

  Q_SLOT void flush();
  Q_SLOT void shutDown();

 private:
  PAGXView* pagxView = nullptr;
};
}  // namespace pag
