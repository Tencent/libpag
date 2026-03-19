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

#include "PAGView.h"
#include <QSGImageNode>
#include "PAGRenderer.h"
#include "RenderThread.h"
#include "tgfx/core/Clock.h"

namespace pag {

PAGView::PAGView(QQuickItem* parent) : ContentView(parent) {
  viewModel = std::make_unique<PAGViewModel>(this);
  auto pagRenderer = std::make_unique<PAGRenderer>(this);
  renderThread = std::make_unique<RenderThread>(this, pagRenderer.get());
  contentRenderer = std::move(pagRenderer);
  renderThread->moveToThread(renderThread.get());
}

PAGView::~PAGView() {
  // Destructor implemented by ContentView
}

ContentViewModel* PAGView::getViewModel() const {
  return viewModel.get();
}

void PAGView::triggerFlush() const {
  QMetaObject::invokeMethod(renderThread.get(), "flush", Qt::QueuedConnection);
}

void PAGView::flush() const {
  if (viewModel->isPlaying()) {
    triggerFlush();
  }
}

void PAGView::initDrawable() {
  if (drawable != nullptr) {
    return;
  }
  drawable = GPUDrawable::MakeFrom(this);
  if (drawable == nullptr) {
    return;
  }
  auto pagSurface = PAGSurface::MakeFrom(drawable);
  viewModel->pagPlayer->setSurface(pagSurface);
  drawable->moveToThread(renderThread.get());
}

void PAGView::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
  if (newGeometry == oldGeometry) {
    return;
  }
  ContentView::geometryChange(newGeometry, oldGeometry);
}

QSGNode* PAGView::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
  if (!renderThread->isRunning()) {
    renderThread->start();
  }

  auto node = static_cast<QSGImageNode*>(oldNode);
  auto texture = drawable->getTexture();
  if (texture) {
    if (node == nullptr) {
      node = window()->createImageNode();
    }
    node->setTexture(texture);
    node->markDirty(QSGNode::DirtyMaterial);
    node->setRect(boundingRect());
  }

  // For content with audio, progress is controlled by audio thread.
  // For content without audio, progress is driven by display time.
  if (!viewModel->hasAudio()) {
    auto timeNow = tgfx::Clock::Now();
    auto displayTime = timeNow - viewModel->lastPlayTime;
    viewModel->lastPlayTime = timeNow;

    if (viewModel->isPlaying_) {
      auto duration = viewModel->pagPlayer->duration();
      if (duration > 0) {
        auto newProgress =
            static_cast<double>(displayTime) / static_cast<double>(duration) + viewModel->progress;
        if (newProgress > 1.0) {
          newProgress = 0.0;
        }
        viewModel->setProgressInternal(newProgress, false);
      }
    } else {
      triggerFlush();
    }
  } else if (viewModel->isPlaying_) {
    triggerFlush();
  }

  return node;
}

}  // namespace pag
