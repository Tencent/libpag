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
#include "rendering/PAGRenderThread.h"
#include "tgfx/core/Clock.h"

namespace pag {

PAGView::PAGView(QQuickItem* parent) : QQuickItem(parent) {
  setFlag(ItemHasContents, true);
  drawable = GPUDrawable::MakeFrom(this);
  pagPlayer = new PAGPlayer();
  auto pagSurface = PAGSurface::MakeFrom(drawable);
  pagPlayer->setSurface(pagSurface);
  renderThread = new PAGRenderThread(this);
  renderThread->moveToThread(renderThread);
  drawable->moveToThread(renderThread);
}

PAGView::~PAGView() {
  QMetaObject::invokeMethod(renderThread, "shutDown", Qt::QueuedConnection);
  renderThread->wait();
  delete renderThread;
  delete pagPlayer;
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

auto PAGView::getTotalFrame() const -> int {
  if (pagFile == nullptr) {
    return 0;
  }
  int totalFrames = static_cast<int>(std::round(getDuration() * pagFile->frameRate() / 1000.0));
  if (totalFrames < 1) {
    totalFrames = 0;
  }
  return totalFrames;
}

auto PAGView::getCurrentFrame() const -> int {
  int totalFrames = getTotalFrame();
  return static_cast<int>(std::round(getProgress() * (totalFrames - 1)));
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

auto PAGView::getDuration() const -> double {
  if (pagPlayer == nullptr) {
    return 0.0;
  }
  return static_cast<double>(pagPlayer->duration()) / 1000.0;
}

auto PAGView::getProgress() const -> double {
  return progress;
}

auto PAGView::getFilePath() const -> QString {
  return filePath;
}

auto PAGView::getBackgroundColor() const -> QColor {
  if (pagFile == nullptr) {
    return QColorConstants::Black;
  }

  auto color = pagFile->getFile()->getRootLayer()->composition->backgroundColor;
  return QColor::fromRgb((int32_t)color.red, (int32_t)color.green, (int32_t)color.blue);
}

auto PAGView::getPreferredSize() const -> QSizeF {
  if (pagFile == nullptr) {
    return {0, 0};
  }

  auto quickWindow = window();
  int pagWidth = getPAGWidth();
  int pagHeight = getPAGHeight();
  auto screen = quickWindow->screen();
  QSize screenSize = screen->availableVirtualSize();
  qreal maxHeight = screenSize.height() * 0.8;
  qreal minHeight = quickWindow->minimumHeight();
  qreal width = 0;
  qreal height = 0;

  if (pagWidth < minHeight) {
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
  if (pagFile == nullptr) {
    return;
  }
  if (this->isPlaying_ == isPlaying) {
    return;
  }
  this->isPlaying_ = isPlaying;
  Q_EMIT isPlayingChanged(isPlaying);
  if (isPlaying) {
    lastPlayTime = tgfx::Clock::Now();
    QMetaObject::invokeMethod(renderThread, "flush", Qt::QueuedConnection);
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
  QMetaObject::invokeMethod(renderThread, "flush", Qt::QueuedConnection);
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

auto PAGView::nextFrame() -> void {
  if (pagFile == nullptr) {
    return;
  }
  setIsPlaying(false);
  auto progress = this->progress + progressPerFrame;
  if (progress > 1) {
    progress = 0.0;
  }
  setProgress(progress);
}

auto PAGView::previousFrame() -> void {
  if (pagFile == nullptr) {
    return;
  }
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
      pagPlayer->setProgress(progress);
      setProgress(progress);
    }
    QMetaObject::invokeMethod(renderThread, "flush", Qt::QueuedConnection);
  }
  return node;
}
}  // namespace pag