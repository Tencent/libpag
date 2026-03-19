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

class ContentView;

/**
 * Unified render thread for both PAG and PAGX content.
 * Delegates format-specific rendering to the appropriate View class.
 */
class RenderThread : public QThread {
  Q_OBJECT
 public:
  enum class ViewType { PAG, PAGX };

  explicit RenderThread(ContentView* view, ViewType type);

  // PAG format signal: includes frame number for animation control
  Q_SIGNAL void frameTimeMetricsReady(int64_t frame, int64_t renderTime, int64_t presentTime,
                                      int64_t imageDecodeTime);

  // PAGX format signal: no frame number since it's static content
  Q_SIGNAL void renderTimeReady(int64_t renderTime, int64_t imageTime, int64_t presentTime);

  Q_SLOT void flush();
  Q_SLOT void shutDown();

 private:
  void flushPAG();
  void flushPAGX();

  ContentView* view = nullptr;
  ViewType viewType = ViewType::PAG;
};
}  // namespace pag
