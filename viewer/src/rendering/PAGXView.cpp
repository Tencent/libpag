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

#include "PAGXView.h"
#include <QSGImageNode>
#include "PAGXRenderer.h"
#include "RenderThread.h"
#include "pagx/PAGXImporter.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/core/Clock.h"

namespace pag {

PAGXView::PAGXView(QQuickItem* parent) : ContentView(parent) {
  auto pagxRenderer = std::make_unique<PAGXRenderer>(this);
  renderThread = std::make_unique<RenderThread>(this, pagxRenderer.get());
  contentRenderer = std::move(pagxRenderer);
  renderThread->moveToThread(renderThread.get());
}

PAGXView::~PAGXView() {
  // Destructor implemented by ContentView
}

void PAGXView::initDrawable() {
  if (drawable != nullptr) {
    return;
  }
  drawable = GPUDrawable::MakeFrom(this, nullptr);
  if (drawable == nullptr) {
    return;
  }
  drawable->moveToThread(renderThread.get());
}

void PAGXView::sizeChangedDelayHandle() {
  resizeTimer->stop();
  sizeChanged = true;
  needsRender = true;
  QMetaObject::invokeMethod(renderThread.get(), "flush", Qt::QueuedConnection);
}

void PAGXView::flush() const {
  if (displayList != nullptr) {
    QMetaObject::invokeMethod(renderThread.get(), "flush", Qt::QueuedConnection);
  }
}

int PAGXView::getWidth() const {
  return pagxWidth;
}

int PAGXView::getHeight() const {
  return pagxHeight;
}

bool PAGXView::hasAnimation() const {
  return totalFrames > 1;
}

bool PAGXView::isPlaying() const {
  return isPlaying_;
}

double PAGXView::getProgress() const {
  return progress;
}

QString PAGXView::getTotalFrame() const {
  return QString::number(totalFrames);
}

QString PAGXView::getCurrentFrame() const {
  auto currentFrame = static_cast<int64_t>(std::floor(progress * static_cast<double>(totalFrames)));
  if (currentFrame >= totalFrames) {
    currentFrame = totalFrames - 1;
  }
  return QString::number(currentFrame);
}

QString PAGXView::getDuration() const {
  if (frameRate <= 0 || totalFrames <= 1) {
    return "0";
  }
  auto durationMs = static_cast<int64_t>(static_cast<double>(totalFrames) / frameRate * 1000.0);
  return QString::number(durationMs);
}

QString PAGXView::getFilePath() const {
  return QString::fromLocal8Bit(currentFilePath.data());
}

QString PAGXView::getDisplayedTime() const {
  auto durationMs = getDuration().toLongLong();
  if (durationMs <= 0) {
    return "0:00";
  }
  auto displayedTimeSeconds =
      static_cast<int64_t>(std::round(progress * static_cast<double>(durationMs) / 1000.0));
  int64_t displayedSeconds = displayedTimeSeconds % 60;
  int64_t displayedMinutes = (displayedTimeSeconds / 60) % 60;
  return QString("%1:%2")
      .arg(displayedMinutes, 2, 10, QChar('0'))
      .arg(displayedSeconds, 2, 10, QChar('0'));
}

QColor PAGXView::getBackgroundColor() const {
  return QColorConstants::White;
}

QSizeF PAGXView::getPreferredSize() const {
  auto quickWindow = window();
  if (quickWindow == nullptr || pagxWidth == 0 || pagxHeight == 0) {
    return {0, 0};
  }
  auto screen = quickWindow->screen();
  QSize screenSize = screen->availableVirtualSize();
  qreal maxHeight = screenSize.height() * 0.8;
  qreal minHeight = quickWindow->minimumHeight();
  qreal width = 0;
  qreal height = 0;

  if (pagxHeight < minHeight) {
    height = minHeight;
    width = pagxWidth * height / pagxHeight;
  } else {
    height = pagxHeight;
    width = pagxWidth;
  }

  if (height > maxHeight) {
    width = width * maxHeight / height;
    height = maxHeight;
  }

  return {width, height};
}

int PAGXView::getEditableTextLayerCount() const {
  return 0;
}

int PAGXView::getEditableImageLayerCount() const {
  return 0;
}

bool PAGXView::getShowVideoFrames() const {
  return false;
}

void PAGXView::setIsPlaying(bool isPlaying) {
  if (!hasAnimation()) {
    isPlaying = false;
  }
  if (isPlaying_ == isPlaying) {
    return;
  }
  isPlaying_ = isPlaying;
  Q_EMIT isPlayingChanged(isPlaying);
  if (isPlaying) {
    lastPlayTime = tgfx::Clock::Now();
    QMetaObject::invokeMethod(renderThread.get(), "flush", Qt::QueuedConnection);
  }
}

void PAGXView::setProgress(double newProgress) {
  if (progress == newProgress) {
    return;
  }
  progress = newProgress;
  Q_EMIT progressChanged(progress);
  needsRender = true;
  QMetaObject::invokeMethod(renderThread.get(), "flush", Qt::QueuedConnection);
}

void PAGXView::setShowVideoFrames(bool) {
}

void PAGXView::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
  if (newGeometry == oldGeometry) {
    return;
  }
  ContentView::geometryChange(newGeometry, oldGeometry);
  if (displayList != nullptr && !needsRender) {
    resizeTimer->start(400);
  }
}

bool PAGXView::setFile(const QString& filePath) {
  auto strPath = std::string(filePath.toLocal8Bit());
  if (filePath.startsWith("file://")) {
    strPath = std::string(QUrl(filePath).toLocalFile().toLocal8Bit());
  }
  auto byteData = pag::ByteData::FromPath(strPath);
  if (byteData == nullptr) {
    return false;
  }

  auto document = pagx::PAGXImporter::FromXML(byteData->data(), byteData->length());
  if (document == nullptr) {
    return false;
  }

  clearContent();
  currentFilePath = strPath;
  pagxDocument = document;
  pagxWidth = static_cast<int>(document->width);
  pagxHeight = static_cast<int>(document->height);

  pagxContentLayer = pagx::LayerBuilder::Build(document.get());
  if (pagxContentLayer == nullptr) {
    clearContent();
    Q_EMIT fileChanged(nullptr);
    Q_EMIT pagxDocumentChanged(nullptr);
    return false;
  }

  updateAnimationState();

  displayList = std::make_unique<tgfx::DisplayList>();
  displayList->root()->addChild(pagxContentLayer);

  sizeChanged = true;
  needsRender = true;
  setSize(getPreferredSize());

  // Trigger render first to display the new content immediately.
  QMetaObject::invokeMethod(renderThread.get(), "flush", Qt::QueuedConnection);

  Q_EMIT fileChanged(nullptr);
  Q_EMIT filePathChanged(QString::fromLocal8Bit(strPath.data()));
  Q_EMIT editableTextLayerCountChanged(0);
  Q_EMIT editableImageLayerCountChanged(0);
  // pagxDocumentChanged is connected with Qt::QueuedConnection, so tree building
  // happens asynchronously and won't block the render.
  Q_EMIT pagxDocumentChanged(pagxDocument);

  return true;
}

void PAGXView::clearContent() {
  pagxDocument = nullptr;
  pagxContentLayer = nullptr;
  displayList = nullptr;
  pagxWidth = 0;
  pagxHeight = 0;
  currentFilePath = {};
  totalFrames = 1;
  frameRate = 0.0f;
  progress = 0.0;
  progressPerFrame = 1.0;
  isPlaying_ = false;
  lastPlayTime = 0;
}

void PAGXView::firstFrame() {
  setIsPlaying(false);
  setProgress(0);
}

void PAGXView::lastFrame() {
  setIsPlaying(false);
  setProgress(1.0);
}

void PAGXView::nextFrame() {
  setIsPlaying(false);
  auto newProgress = progress + progressPerFrame;
  if (newProgress > 1.0) {
    newProgress = 0.0;
  }
  setProgress(newProgress);
}

void PAGXView::previousFrame() {
  setIsPlaying(false);
  auto newProgress = progress - progressPerFrame;
  if (newProgress < 0.0) {
    newProgress = 1.0;
  }
  setProgress(newProgress);
}

QSGNode* PAGXView::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
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

  return node;
}

void PAGXView::updateAnimationState() {
  // TODO: Read timeline properties from PAGXDocument when available.
  totalFrames = 1;
  frameRate = 0.0f;
  progress = 0.0;
  progressPerFrame = 1.0;
  isPlaying_ = false;
  lastPlayTime = 0;
}

}  // namespace pag
