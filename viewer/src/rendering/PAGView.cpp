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

auto PAGView::flush() const -> void {
  if (pagFile != nullptr && isPlaying_) {
    QMetaObject::invokeMethod(renderThread.get(), "flush", Qt::QueuedConnection);
  }
}

PAGView::~PAGView() {
  QMetaObject::invokeMethod(renderThread.get(), "shutDown", Qt::QueuedConnection);
  renderThread->wait();
}

auto PAGView::getPAGWidth() const -> int {
  if (pagFile == nullptr) {
    return -1;
  }
  return pagFile->width();
}

auto PAGView::getPAGHeight() const -> int {
  if (pagFile == nullptr) {
    return -1;
  }
  return pagFile->height();
}

auto PAGView::getTotalFrame() const -> QString {
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

auto PAGView::getCurrentFrame() const -> QString {
  int64_t totalFrames = getTotalFrame().toLongLong();
  int64_t currentFrame = static_cast<int64_t>(std::round(getProgress() * (totalFrames - 1)));
  return QString::number(currentFrame);
}

auto PAGView::isPlaying() const -> bool {
  return isPlaying_;
}

auto PAGView::getShowVideoFrames() const -> bool {
  if (pagPlayer == nullptr) {
    return false;
  }
  return pagPlayer->videoEnabled();
}

auto PAGView::getDuration() const -> QString {
  if (pagPlayer == nullptr) {
    return "0";
  }
  return QString::number(pagPlayer->duration() / 1000);
}

auto PAGView::getProgress() const -> double {
  return progress;
}

auto PAGView::getFilePath() const -> QString {
  return filePath;
}

auto PAGView::getDisplayedTime() const -> QString {
  int64_t displayedTime =
      static_cast<int64_t>(std::round(getProgress() * getDuration().toLongLong() / 1000.0));
  int64_t displayedSeconds = displayedTime % 60;
  int64_t displayedMinutes = (displayedTime / 60) % 60;
  return QString("%1:%2")
      .arg(displayedMinutes, 2, 10, QChar('0'))
      .arg(displayedSeconds, 2, 10, QChar('0'));
}

auto PAGView::getBackgroundColor() const -> QColor {
  if (pagFile == nullptr) {
    return QColorConstants::Black;
  }

  auto color = pagFile->getFile()->getRootLayer()->composition->backgroundColor;
  return QColor::fromRgb((int32_t)color.red, (int32_t)color.green, (int32_t)color.blue);
}

auto PAGView::getPreferredSize() const -> QSizeF {
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

auto PAGView::setIsPlaying(bool isPlaying) -> void {
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

auto PAGView::setShowVideoFrames(bool isShow) -> void {
  if (pagPlayer == nullptr) {
    return;
  }
  pagPlayer->setVideoEnabled(isShow);
}

auto PAGView::setProgress(double progress) -> void {
  if (this->progress == progress) {
    return;
  }
  pagPlayer->setProgress(progress);
  this->progress = progress;
  Q_EMIT progressChanged(progress);
  QMetaObject::invokeMethod(renderThread.get(), "flush", Qt::QueuedConnection);
}

auto PAGView::setFile(const QString& filePath) -> bool {
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
  this->filePath = strPath.c_str();
  pagPlayer->setComposition(pagFile);
  setSize(getPreferredSize());
  progressPerFrame = 1.0 / (pagFile->frameRate() * pagFile->duration() / 1000000);
  Q_EMIT fileChanged(pagFile, strPath);
  setProgress(0);
  setIsPlaying(true);

  return true;
}

auto PAGView::firstFrame() -> void {
  setIsPlaying(false);
  setProgress(0);
}

auto PAGView::lastFrame() -> void {
  setIsPlaying(false);
  setProgress(1);
}

auto PAGView::nextFrame() -> void {
  setIsPlaying(false);
  auto progress = this->progress + progressPerFrame;
  if (progress > 1) {
    progress = 0.0;
  }
  setProgress(progress);
}

auto PAGView::previousFrame() -> void {
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

  if ((pagFile != nullptr) && isPlaying_) {
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

auto PAGView::getRenderThread() const -> PAGRenderThread* {
  return renderThread.get();
}
}  // namespace pag