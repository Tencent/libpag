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

#include <QQuickItem>
#include <QTimer>
#include <atomic>
#include <memory>
#include "ContentViewModel.h"
#include "IContentRenderer.h"
#include "platform/qt/GPUDrawable.h"

namespace pag {

class RenderThread;

/**
 * Base class for content views that render PAG or PAGX files via the Qt Scene Graph.
 * Manages the render thread lifecycle, drawable, and resize debouncing.
 * All business state is owned by the subclass ViewModel, accessible via getViewModel().
 */
class ContentView : public QQuickItem {
  Q_OBJECT
 public:
  Q_PROPERTY(ContentViewModel* viewModel READ getViewModel CONSTANT)

  explicit ContentView(QQuickItem* parent = nullptr);
  ~ContentView() override;

  virtual ContentViewModel* getViewModel() const = 0;
  RenderThread* getRenderThread() const;

  Q_SLOT virtual void flush() const = 0;

  friend class RenderThread;

 protected:
  Q_SLOT virtual void sizeChangedDelayHandle();
  Q_SLOT void onWindowChanged(QQuickWindow* win);

  virtual void initDrawable();
  virtual void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

  std::shared_ptr<GPUDrawable> drawable = nullptr;
  std::unique_ptr<IContentRenderer> contentRenderer = nullptr;
  std::unique_ptr<RenderThread> renderThread = nullptr;
  std::unique_ptr<QTimer> resizeTimer = nullptr;
  std::atomic_bool sizeChanged = false;
};

}  // namespace pag
