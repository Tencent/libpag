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

#include "PAGView.h"
#include <QGuiApplication>
#include <QQuickWindow>
#include <QSGImageNode>
#include <QScreen>
#include <QThread>
#include "pag/file.h"
#include "platform/qt/GPUDrawable.h"
#include "tgfx/utils/Clock.h"

namespace pag {
class RenderThread : public QThread {
  Q_OBJECT
 public:
  explicit RenderThread(PAGView* pagView) : pagView(pagView) {
  }

  Q_SLOT
  void flush() {
    pagView->pagPlayer->flush();
    QMetaObject::invokeMethod(pagView, "update", Qt::QueuedConnection);
  }

  Q_SLOT
  void shutDown() {
    // Stop event processing, move the thread to GUI and make sure it is deleted.
    exit();
    if (QGuiApplication::instance()) {
      auto mainThread = QGuiApplication::instance()->thread();
      if (pagView->drawable) {
        pagView->drawable->moveToThread(mainThread);
      }
      moveToThread(mainThread);
    }
  }

 private:
  PAGView* pagView = nullptr;
};

PAGView::PAGView(QQuickItem* parent) : QQuickItem(parent) {
  setFlag(ItemHasContents, true);
  drawable = GPUDrawable::MakeFrom(this);
  pagPlayer = new PAGPlayer();
  auto pagSurface = PAGSurface::MakeFrom(drawable);
  pagPlayer->setSurface(pagSurface);
  renderThread = new RenderThread(this);
  renderThread->moveToThread(renderThread);
  drawable->moveToThread(renderThread);
}

PAGView::~PAGView() {
  QMetaObject::invokeMethod(renderThread, "shutDown", Qt::QueuedConnection);
  renderThread->wait();
  delete renderThread;
  delete pagPlayer;
}

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))

void PAGView::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
  if (newGeometry == oldGeometry) {
    return;
  }
  QQuickItem::geometryChange(newGeometry, oldGeometry);
  onSizeChanged();
}

#else

void PAGView::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry) {
  if (newGeometry == oldGeometry) {
    return;
  }
  QQuickItem::geometryChanged(newGeometry, oldGeometry);
  onSizeChanged();
}

#endif

void PAGView::setFile(const std::shared_ptr<PAGFile> pagFile) {
  pagPlayer->setComposition(pagFile);
}

QSGNode* PAGView::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
  if (!renderThread->isRunning()) {
    lastDevicePixelRatio = window()->devicePixelRatio();
    renderThread->start();
  }

  if (lastDevicePixelRatio != window()->devicePixelRatio()) {
    lastDevicePixelRatio = window()->devicePixelRatio();
    onSizeChanged();
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

  if (isPlaying) {
    auto duration = pagPlayer->duration();
    if (duration > 0) {
      auto playTime = (tgfx::Clock::Now() - startTime) % duration;
      auto progress = static_cast<double>(playTime) / static_cast<double>(duration);
      pagPlayer->setProgress(progress);
    }
    QMetaObject::invokeMethod(renderThread, "flush", Qt::QueuedConnection);
  }
  return node;
}

void PAGView::onSizeChanged() {
  auto pagSurface = pagPlayer->getSurface();
  if (pagSurface) {
    pagSurface->updateSize();
  }
}
}  // namespace pag

#include "PAGView.moc"