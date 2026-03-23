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
#include <memory>
#include "IContentRenderer.h"

namespace pag {

class ContentView;

/**
 * Unified render thread for both PAG and PAGX content.
 * Delegates all format-specific rendering to an IContentRenderer implementation.
 */
class RenderThread : public QThread {
  Q_OBJECT
 public:
  explicit RenderThread(ContentView* view, std::unique_ptr<IContentRenderer> renderer);

  /**
   * Sets the drawable on the renderer. Call from main thread after drawable is initialized.
   */
  void setDrawable(GPUDrawable* drawable);

  /**
   * Emitted after each rendered frame to notify the view to present the new content.
   */
  Q_SIGNAL void rendered();

  /**
   * Emitted after each rendered frame with timing metrics.
   * currentFrame is -1 for content types that have no frame index (e.g. static PAGX).
   */
  Q_SIGNAL void renderMetricsReady(int64_t renderTime, int64_t presentTime, int64_t imageDecodeTime,
                                   int64_t currentFrame);

  Q_SLOT void flush();
  Q_SLOT void shutDown();

 private:
  ContentView* view = nullptr;
  std::unique_ptr<IContentRenderer> renderer = nullptr;
};

}  // namespace pag
