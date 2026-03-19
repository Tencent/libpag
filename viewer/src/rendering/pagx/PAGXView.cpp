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

#include "rendering/pagx/PAGXView.h"
#include "rendering/pagx/PAGXRenderer.h"
#include "rendering/RenderThread.h"

namespace pag {

PAGXView::PAGXView(QQuickItem* parent) : ContentView(parent) {
  viewModel = std::make_unique<PAGXViewModel>(this);
  auto pagxRenderer = std::make_unique<PAGXRenderer>(this);
  renderThread = std::make_unique<RenderThread>(this, pagxRenderer.get());
  contentRenderer = std::move(pagxRenderer);
  renderThread->moveToThread(renderThread.get());
}

PAGXView::~PAGXView() {
  // Destructor implemented by ContentView
}

ContentViewModel* PAGXView::getViewModel() const {
  return viewModel.get();
}

void PAGXView::initDrawable() {
  if (drawable != nullptr) {
    return;
  }
  drawable = GPUDrawable::MakeFrom(this, nullptr);
  if (drawable == nullptr) {
    return;
  }
  if (renderThread != nullptr) {
    drawable->moveToThread(renderThread.get());
  }
}

void PAGXView::sizeChangedDelayHandle() {
  resizeTimer->stop();
  sizeChanged = true;
  viewModel->needsRender = true;
  if (renderThread != nullptr) {
    triggerFlush();
  }
}

void PAGXView::flush() const {
  if (viewModel->needsRender) {
    triggerFlush();
  }
}

void PAGXView::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
  if (newGeometry == oldGeometry) {
    return;
  }
  ContentView::geometryChange(newGeometry, oldGeometry);
  if (viewModel->displayList != nullptr) {
    resizeTimer->start(400);
  }
}

QSGNode* PAGXView::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
  if (!renderThread->isRunning()) {
    renderThread->start();
  }

  return updateTextureNode(oldNode);
}

}  // namespace pag
