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
#include <QQuickWindow>
#include <QSGImageNode>
#include <algorithm>
#include "RenderThread.h"

namespace pag {

ContentView::ContentView(QQuickItem* parent) : QQuickItem(parent) {
  setFlag(ItemHasContents, true);
  resizeTimer = std::make_unique<QTimer>();
  connect(resizeTimer.get(), &QTimer::timeout, this, &ContentView::sizeChangedDelayHandle);
  connect(this, &QQuickItem::windowChanged, this, &ContentView::onWindowChanged);
}

ContentView::~ContentView() {
  resizeTimer->stop();
  releaseDrawable();
}

void ContentView::onWindowChanged(QQuickWindow* win) {
  if (win != nullptr) {
    initDrawable();
  }
}

void ContentView::itemChange(ItemChange change, const ItemChangeData& value) {
  QQuickItem::itemChange(change, value);
  if (change == ItemSceneChange && value.window == nullptr) {
    releaseDrawable();
  }
}

void ContentView::initDrawable() {
  // Base implementation does nothing, subclasses will override
}

void ContentView::releaseDrawable() {
  if (renderThread != nullptr && renderThread->isRunning()) {
    QMetaObject::invokeMethod(renderThread.get(), "shutDown", Qt::QueuedConnection);
    renderThread->wait();
  }
  drawable = nullptr;
}

void ContentView::prepareForRemoval() {
  // Called explicitly before QML Loader switches components, because itemChange alone
  // fires after the item is detached from the window. RenderThread::shutDown() needs
  // the item still attached so it can move the drawable back to the main thread safely.
  releaseDrawable();
}

void ContentView::zoomAt(double factor, double x, double y) {
  auto pixelRatio = window() == nullptr ? 1.0 : window()->devicePixelRatio();
  getViewModel()->zoomAt(factor, x * pixelRatio, y * pixelRatio);
}

void ContentView::panBy(double deltaX, double deltaY) {
  auto pixelRatio = window() == nullptr ? 1.0 : window()->devicePixelRatio();
  getViewModel()->panBy(deltaX * pixelRatio, deltaY * pixelRatio);
}

void ContentView::resetView() {
  getViewModel()->resetView();
}

void ContentView::sizeChangedDelayHandle() {
  resizeTimer->stop();
  contentResizing = false;
  if (sizeChanged) {
    return;
  }
  sizeChanged = true;
  onSizeChangedDelayHandled();
  if (renderThread != nullptr) {
    QMetaObject::invokeMethod(renderThread.get(), "flush", Qt::QueuedConnection);
  }
}

void ContentView::beginContentResize() {
  // The timer doubles as the end marker of the content resize: it fires once the geometry stopped
  // changing, and also covers the case where the item already had the preferred size and no
  // geometry change follows, so the mode can never outlive the fitting of the new content.
  resizeTimer->start(400);
  contentResizing = true;
}

void ContentView::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
  if (newGeometry == oldGeometry) {
    return;
  }
  QQuickItem::geometryChange(newGeometry, oldGeometry);
  if (contentResizing) {
    // The intermediate geometries produced while the window is fitted to newly loaded content are
    // not worth debouncing: waiting for the timer keeps the previous texture stretched onto every
    // one of them, which is visible as a wrong aspect ratio right after a file is opened.
    sizeChanged = true;
    triggerFlush();
  }
  resizeTimer->start(400);
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
      node->setRect(textureRect(texture));
    }
  }
  return node;
}

QRectF ContentView::textureRect(QSGTexture* texture) const {
  auto rect = boundingRect();
  auto textureSize = texture->textureSize();
  auto viewModel = getViewModel();
  if (viewModel == nullptr || textureSize.isEmpty() || rect.isEmpty()) {
    return rect;
  }
  auto contentWidth = static_cast<double>(viewModel->getWidth());
  auto contentHeight = static_cast<double>(viewModel->getHeight());
  if (contentWidth <= 0 || contentHeight <= 0) {
    return rect;
  }
  auto textureWidth = static_cast<double>(textureSize.width());
  auto textureHeight = static_cast<double>(textureSize.height());
  // Both renderers fit the content into the surface at a uniform scale and center it, so the
  // content area of the texture is known from its size alone. Placing that area at the same fit in
  // the current canvas makes a stale frame look like the frame that is about to be rendered.
  auto textureFit = std::min(textureWidth / contentWidth, textureHeight / contentHeight);
  auto canvasFit = std::min(rect.width() / contentWidth, rect.height() / contentHeight);
  if (textureFit <= 0) {
    return rect;
  }
  auto transform = viewModel->getViewTransform();
  auto pixelRatio = window() == nullptr ? 1.0 : window()->devicePixelRatio();
  auto scale = canvasFit / textureFit;
  auto contentX =
      ((textureWidth - contentWidth * textureFit) * 0.5) * transform.zoomScale + transform.offsetX;
  auto contentY = ((textureHeight - contentHeight * textureFit) * 0.5) * transform.zoomScale +
                  transform.offsetY;
  auto targetX = ((rect.width() - contentWidth * canvasFit) * 0.5) * transform.zoomScale +
                 transform.offsetX / pixelRatio;
  auto targetY = ((rect.height() - contentHeight * canvasFit) * 0.5) * transform.zoomScale +
                 transform.offsetY / pixelRatio;
  auto width = textureWidth * scale;
  auto height = textureHeight * scale;
  return {rect.x() + targetX - contentX * scale, rect.y() + targetY - contentY * scale, width,
          height};
}

}  // namespace pag
