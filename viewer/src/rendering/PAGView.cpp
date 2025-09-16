/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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
#include "pag/file.h"
#include "tgfx/core/Clock.h"

namespace pag {

PAGView::PAGView(QQuickItem* parent) : QQuickItem(parent) {
  setFlag(ItemHasContents, true);
  drawable = GPUDrawable::MakeFrom(this);
  pagPlayer = std::make_unique<PAGPlayer>();
  auto pagSurface = PAGSurface::MakeFrom(drawable);
  pagPlayer->setSurface(pagSurface);
  renderThread = std::make_unique<PAGRenderThread>(this);
  renderThread->moveToThread(renderThread.get());
  drawable->moveToThread(renderThread.get());
}

void PAGView::flush() const {
  if (isPlaying_) {
    QMetaObject::invokeMethod(renderThread.get(), "flush", Qt::QueuedConnection);
  }
}

PAGView::~PAGView() {
  QMetaObject::invokeMethod(renderThread.get(), "shutDown", Qt::QueuedConnection);
  renderThread->wait();
}

int PAGView::getPAGWidth() const {
  if (pagFile == nullptr) {
    return 0;
  }
  return pagFile->width();
}

int PAGView::getPAGHeight() const {
  if (pagFile == nullptr) {
    return 0;
  }
  return pagFile->height();
}

int PAGView::getEditableTextLayerCount() const {
  return editableTextLayerCount;
}

int PAGView::getEditableImageLayerCount() const {
  return editableImageLayerCount;
}

QString PAGView::getTotalFrame() const {
  if (pagFile == nullptr) {
    return "0";
  }
  int64_t totalFrames =
      static_cast<int64_t>(std::round(getDuration().toLongLong() * pagFile->frameRate() / 1000.0));
  if (totalFrames < 1) {
    totalFrames = 0;
  }
  return QString::number(totalFrames);
}

QString PAGView::getCurrentFrame() const {
  int64_t totalFrames = getTotalFrame().toLongLong();
  int64_t currentFrame = static_cast<int64_t>(std::round(getProgress() * (totalFrames - 1)));
  return QString::number(currentFrame);
}

bool PAGView::isPlaying() const {
  return isPlaying_;
}

bool PAGView::getShowVideoFrames() const {
  if (pagPlayer == nullptr) {
    return false;
  }
  return pagPlayer->videoEnabled();
}

QString PAGView::getDuration() const {
  if (pagPlayer == nullptr) {
    return "0";
  }
  return QString::number(pagPlayer->duration() / 1000);
}

double PAGView::getProgress() const {
  return progress;
}

QString PAGView::getFilePath() const {
  if (pagFile == nullptr) {
    return "";
  }

  return QString::fromStdString(pagFile->path());
}

QString PAGView::getDisplayedTime() const {
  int64_t displayedTime =
      static_cast<int64_t>(std::round(getProgress() * getDuration().toLongLong() / 1000.0));
  int64_t displayedSeconds = displayedTime % 60;
  int64_t displayedMinutes = (displayedTime / 60) % 60;
  return QString("%1:%2")
      .arg(displayedMinutes, 2, 10, QChar('0'))
      .arg(displayedSeconds, 2, 10, QChar('0'));
}

QColor PAGView::getBackgroundColor() const {
  if (pagFile == nullptr) {
    return QColorConstants::Black;
  }

  auto color = pagFile->getFile()->getRootLayer()->composition->backgroundColor;
  return QColor::fromRgb((int32_t)color.red, (int32_t)color.green, (int32_t)color.blue);
}

QSizeF PAGView::getPreferredSize() const {
  auto quickWindow = window();
  int pagWidth = getPAGWidth();
  int pagHeight = getPAGHeight();
  auto screen = quickWindow->screen();
  QSize screenSize = screen->availableVirtualSize();
  qreal maxHeight = screenSize.height() * 0.8;
  qreal minHeight = quickWindow->minimumHeight();
  qreal width = 0;
  qreal height = 0;

  if (pagHeight < minHeight) {
    height = minHeight;
    width = pagWidth * height / pagHeight;
  } else {
    height = pagHeight;
    width = pagWidth;
  }

  if (height > maxHeight) {
    width = width * maxHeight / height;
    height = maxHeight;
  }

  return {width, height};
}

void PAGView::setIsPlaying(bool isPlaying) {
  if (this->isPlaying_ == isPlaying) {
    return;
  }
  this->isPlaying_ = isPlaying;
  Q_EMIT isPlayingChanged(isPlaying);
  if (isPlaying) {
    lastPlayTime = tgfx::Clock::Now();
    QMetaObject::invokeMethod(renderThread.get(), "flush", Qt::QueuedConnection);
  }
}

void PAGView::setShowVideoFrames(bool isShow) {
  if (pagPlayer == nullptr) {
    return;
  }
  pagPlayer->setVideoEnabled(isShow);
}

void PAGView::setProgress(double progress) {
  if (this->progress == progress) {
    return;
  }
  pagPlayer->setProgress(progress);
  this->progress = progress;
  Q_EMIT progressChanged(progress);
  QMetaObject::invokeMethod(renderThread.get(), "flush", Qt::QueuedConnection);
}

bool PAGView::setFile(const QString& filePath) {
  auto strPath = std::string(filePath.toLocal8Bit());
  if (filePath.startsWith("file://")) {
    strPath = std::string(QUrl(filePath).toLocalFile().toLocal8Bit());
  }
  auto byteData = pag::ByteData::FromPath(strPath);
  if (byteData == nullptr) {
    return false;
  }
  auto newPagFile = pag::PAGFile::Load(byteData->data(), byteData->length());
  if (newPagFile == nullptr) {
    return false;
  }
  setIsPlaying(false);
  pagFile = newPagFile;
  pagFile->getFile()->path = strPath;
  pagPlayer->setComposition(pagFile);
  setSize(getPreferredSize());
  progressPerFrame = 1.0 / (pagFile->frameRate() * pagFile->duration() / 1000000);
  Q_EMIT fileChanged(pagFile->getFile());
  Q_EMIT filePathChanged(strPath);
  Q_EMIT pagFileChanged(pagFile);
  pagPlayer->setProgress(0);
  setProgress(0);
  setIsPlaying(true);

  editableTextLayerCount = static_cast<int>(pagFile->getEditableIndices(LayerType::Text).size());
  editableImageLayerCount = static_cast<int>(pagFile->getEditableIndices(LayerType::Image).size());
  Q_EMIT editableTextLayerCountChanged(editableTextLayerCount);
  Q_EMIT editableImageLayerCountChanged(editableImageLayerCount);

  return true;
}

void PAGView::firstFrame() {
  setIsPlaying(false);
  setProgress(0);
}

void PAGView::lastFrame() {
  setIsPlaying(false);
  setProgress(1);
}

void PAGView::nextFrame() {
  setIsPlaying(false);
  auto progress = this->progress + progressPerFrame;
  if (progress > 1) {
    progress = 0.0;
  }
  setProgress(progress);
}

void PAGView::previousFrame() {
  setIsPlaying(false);
  auto progress = this->progress - progressPerFrame;
  if (progress < 0) {
    progress = 1.0;
  }
  setProgress(progress);
}

QSGNode* PAGView::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
  if (!renderThread->isRunning()) {
    renderThread->start();
  }

  if (lastWidth != width() || lastHeight != height() ||
      lastPixelRatio != window()->devicePixelRatio()) {
    lastWidth = width();
    lastHeight = height();
    lastPixelRatio = window()->devicePixelRatio();
    auto pagSurface = pagPlayer->getSurface();
    if (pagSurface) {
      pagSurface->updateSize();
    }
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

  auto timeNow = tgfx::Clock::Now();
  auto displayTime = timeNow - lastPlayTime;
  lastPlayTime = timeNow;

  if (isPlaying_) {
    auto duration = pagPlayer->duration();
    if (duration > 0) {
      auto progress =
          static_cast<double>(displayTime) / static_cast<double>(duration) + this->progress;
      if (progress > 1) {
        progress = 0.0;
      }
      setProgress(progress);
    }
  }
  return node;
}

PAGRenderThread* PAGView::getRenderThread() const {
  return renderThread.get();
}
}  // namespace pag