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

#include "ContentView.h"
#include <QDateTime>
#include <QQuickWindow>
#include <QSGImageNode>
#include "RenderThread.h"

namespace pag {

ContentView::ContentView(QQuickItem* parent) : QQuickItem(parent) {
  setFlag(ItemHasContents, true);
  resizeTimer = std::make_unique<QTimer>();
  connect(resizeTimer.get(), &QTimer::timeout, this, &ContentView::sizeChangedDelayHandle);
  if (window() != nullptr) {
    initDrawable();
  } else {
    connect(this, &QQuickItem::windowChanged, this, &ContentView::onWindowChanged);
  }
}

ContentView::~ContentView() {
  if (renderThread != nullptr && renderThread->isRunning()) {
    QMetaObject::invokeMethod(renderThread.get(), "shutDown", Qt::QueuedConnection);
    renderThread->wait();
  }
}

void ContentView::onWindowChanged(QQuickWindow* win) {
  if (win != nullptr) {
    disconnect(this, &QQuickItem::windowChanged, this, &ContentView::onWindowChanged);
    initDrawable();
  }
}

void ContentView::initDrawable() {
  // Base implementation does nothing, subclasses will override
}

void ContentView::sizeChangedDelayHandle() {
  resizeTimer->stop();
  sizeChanged = true;
  onSizeChangedDelayHandled();
  if (renderThread != nullptr) {
    QMetaObject::invokeMethod(renderThread.get(), "flush", Qt::QueuedConnection);
  }
}

void ContentView::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
  if (newGeometry == oldGeometry) {
    return;
  }
  QQuickItem::geometryChange(newGeometry, oldGeometry);
  // Skip starting the timer if:
  // 1. We're within the skip window (subclass handled size change immediately)
  // 2. sizeChanged is already true (a size change is pending)
  // This prevents redundant updateSize() calls that invalidate the surface.
  auto now = QDateTime::currentMSecsSinceEpoch();
  if (now < skipResizeTimerUntil) {
    return;
  }
  if (!sizeChanged) {
    resizeTimer->start(400);
  }
}

RenderThread* ContentView::getRenderThread() const {
  return renderThread.get();
}

bool ContentView::takeSizeChanged() {
  return sizeChanged.exchange(false);
}

GPUDrawable* ContentView::getDrawable() const {
  return drawable.get();
}

void ContentView::triggerFlush() const {
  if (renderThread != nullptr) {
    QMetaObject::invokeMethod(renderThread.get(), "flush", Qt::QueuedConnection);
  }
}

QSGNode* ContentView::updateTextureNode(QSGNode* oldNode) {
  auto node = static_cast<QSGImageNode*>(oldNode);
  if (drawable != nullptr) {
    auto texture = drawable->getTexture();
    if (texture) {
      if (node == nullptr) {
        node = window()->createImageNode();
      }
      node->setTexture(texture);
      node->markDirty(QSGNode::DirtyMaterial);
      node->setRect(boundingRect());
    }
  }
  return node;
}

}  // namespace pag
