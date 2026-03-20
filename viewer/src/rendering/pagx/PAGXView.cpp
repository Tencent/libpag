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
#include "rendering/RenderThread.h"
#include "rendering/pagx/PAGXRenderer.h"

namespace pag {

PAGXView::PAGXView(QQuickItem* parent) : ContentView(parent) {
  viewModel = std::make_unique<PAGXViewModel>();
  connect(viewModel.get(), &PAGXViewModel::requestFlush, this, &PAGXView::triggerFlush);
  connect(viewModel.get(), &PAGXViewModel::requestSizeChanged, this,
          &PAGXView::onRequestSizeChanged);
  connect(viewModel.get(), &PAGXViewModel::preferredSizeChanged, this,
          &PAGXView::onPreferredSizeChanged);
  auto pagxRenderer = std::make_unique<PAGXRenderer>(viewModel.get());
  pagxRenderer_ = pagxRenderer.get();
  renderThread =
      std::make_unique<RenderThread>(this, std::move(pagxRenderer));
  connect(renderThread.get(), &RenderThread::rendered, this, &PAGXView::update,
          Qt::QueuedConnection);
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
  if (pagxRenderer_ != nullptr) {
    pagxRenderer_->setDrawable(drawable.get());
  }
  viewModel->setWindow(this->window());
  if (renderThread != nullptr) {
    drawable->moveToThread(renderThread.get());
  }
}

void PAGXView::onRequestSizeChanged() {
  sizeChanged = true;
  viewModel->markNeedsRender();
  triggerFlush();
}

void PAGXView::onPreferredSizeChanged() {
  setSize(viewModel->getPreferredSize());
}

void PAGXView::sizeChangedDelayHandle() {
  ContentView::sizeChangedDelayHandle();
  viewModel->markNeedsRender();
}

void PAGXView::flush() const {
  if (viewModel->hasContent()) {
    triggerFlush();
  }
}

QSGNode* PAGXView::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
  if (!renderThread->isRunning()) {
    renderThread->start();
  }

  return updateTextureNode(oldNode);
}

}  // namespace pag
