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

#include "rendering/pagx/PAGXViewModel.h"
#include <QQuickWindow>
#include "pagx/PAGXImporter.h"
#include "renderer/LayerBuilder.h"

namespace pag {

PAGXViewModel::PAGXViewModel(QObject* parent) : ContentViewModel(parent) {
}

int PAGXViewModel::getWidth() const {
  return pagxWidth;
}

int PAGXViewModel::getHeight() const {
  return pagxHeight;
}

bool PAGXViewModel::hasAnimation() const {
  return totalFrames > 1;
}

bool PAGXViewModel::isPlaying() const {
  return isPlaying_;
}

double PAGXViewModel::getProgress() const {
  return progress;
}

QString PAGXViewModel::getTotalFrame() const {
  return QString::number(totalFrames);
}

QString PAGXViewModel::getCurrentFrame() const {
  auto currentFrame = static_cast<int64_t>(std::floor(progress * static_cast<double>(totalFrames)));
  if (currentFrame >= totalFrames) {
    currentFrame = totalFrames - 1;
  }
  return QString::number(currentFrame);
}

QString PAGXViewModel::getDuration() const {
  if (frameRate <= 0 || totalFrames <= 1) {
    return "0";
  }
  auto durationMs = static_cast<int64_t>(static_cast<double>(totalFrames) / frameRate * 1000.0);
  return QString::number(durationMs);
}

QString PAGXViewModel::getFilePath() const {
  return QString::fromLocal8Bit(currentFilePath.data());
}

QString PAGXViewModel::getDisplayedTime() const {
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

QColor PAGXViewModel::getBackgroundColor() const {
  return QColorConstants::White;
}

QSizeF PAGXViewModel::getPreferredSize() const {
  if (window == nullptr || pagxWidth == 0 || pagxHeight == 0) {
    return {0, 0};
  }
  auto screen = window->screen();
  QSize screenSize = screen->availableVirtualSize();
  qreal maxHeight = screenSize.height() * 0.8;
  qreal minHeight = window->minimumHeight();
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

int PAGXViewModel::getEditableTextLayerCount() const {
  return 0;
}

int PAGXViewModel::getEditableImageLayerCount() const {
  return 0;
}

bool PAGXViewModel::getShowVideoFrames() const {
  return false;
}

void PAGXViewModel::setWindow(QQuickWindow* win) {
  window = win;
}

bool PAGXViewModel::takeNeedsRender() {
  return needsRender.exchange(false);
}

void PAGXViewModel::markNeedsRender() {
  needsRender = true;
}

tgfx::DisplayList* PAGXViewModel::getDisplayList() const {
  return displayList.get();
}

tgfx::Layer* PAGXViewModel::getContentLayer() const {
  return pagxContentLayer.get();
}

int PAGXViewModel::getContentWidth() const {
  return pagxWidth;
}

int PAGXViewModel::getContentHeight() const {
  return pagxHeight;
}

void PAGXViewModel::setIsPlaying(bool isPlaying) {
  if (!hasAnimation()) {
    isPlaying = false;
  }
  if (isPlaying_ == isPlaying) {
    return;
  }
  isPlaying_ = isPlaying;
  Q_EMIT isPlayingChanged(isPlaying);
  if (isPlaying) {
    Q_EMIT requestFlush();
  }
}

void PAGXViewModel::setProgress(double newProgress) {
  if (progress == newProgress) {
    return;
  }
  progress = newProgress;
  Q_EMIT progressChanged(progress);
  needsRender = true;
  Q_EMIT requestFlush();
}

void PAGXViewModel::setShowVideoFrames(bool) {
}

bool PAGXViewModel::loadFile(const QString& filePath) {
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

  needsRender = true;
  Q_EMIT fileChanged(nullptr);
  Q_EMIT filePathChanged(QString::fromLocal8Bit(strPath.data()));
  Q_EMIT widthChanged(pagxWidth);
  Q_EMIT heightChanged(pagxHeight);
  Q_EMIT totalFrameChanged();
  Q_EMIT preferredSizeChanged();
  Q_EMIT editableTextLayerCountChanged(0);
  Q_EMIT editableImageLayerCountChanged(0);
  Q_EMIT requestSizeChanged();
  // pagxDocumentChanged is connected with Qt::QueuedConnection, so tree building
  // happens asynchronously and won't block the render.
  Q_EMIT pagxDocumentChanged(pagxDocument);

  return true;
}

void PAGXViewModel::firstFrame() {
  setIsPlaying(false);
  setProgress(0);
}

void PAGXViewModel::lastFrame() {
  setIsPlaying(false);
  setProgress(1.0);
}

void PAGXViewModel::nextFrame() {
  setIsPlaying(false);
  auto newProgress = progress + progressPerFrame;
  if (newProgress > 1.0) {
    newProgress = 0.0;
  }
  setProgress(newProgress);
}

void PAGXViewModel::previousFrame() {
  setIsPlaying(false);
  auto newProgress = progress - progressPerFrame;
  if (newProgress < 0.0) {
    newProgress = 1.0;
  }
  setProgress(newProgress);
}

void PAGXViewModel::clearContent() {
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
  needsRender = false;
}

void PAGXViewModel::updateAnimationState() {
  // TODO: Read timeline properties from PAGXDocument when available.
  totalFrames = 1;
  frameRate = 0.0f;
  progress = 0.0;
  progressPerFrame = 1.0;
  isPlaying_ = false;
}

}  // namespace pag
