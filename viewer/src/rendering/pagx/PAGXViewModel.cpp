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
#include <QFile>
#include <QMetaObject>
#include <QQuickWindow>
#include <cmath>
#include "pag/pag.h"
#include "pagx/PAGXImporter.h"

namespace pag {

PAGXViewModel::PAGXViewModel(QObject* parent) : ContentViewModel(parent) {
  xmlLinesModel = new XmlLinesModel(this);
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
  if (screen == nullptr) {
    return {0, 0};
  }
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

ContentViewModel::ContentType PAGXViewModel::getContentType() const {
  return ContentType::PAGX;
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

void PAGXViewModel::updateProgressFromRender(double newProgress, uint64_t generation) {
  QMetaObject::invokeMethod(this, "applyProgressFromRender", Qt::QueuedConnection,
                            Q_ARG(double, newProgress),
                            Q_ARG(quint64, static_cast<quint64>(generation)));
}

void PAGXViewModel::applyProgressFromRender(double newProgress, quint64 generation) {
  // Ignore a progress update from an earlier playback session; content may have been reloaded.
  if (generation != playbackGeneration.load()) {
    return;
  }
  if (std::abs(progress - newProgress) < 1e-9) {
    return;
  }
  progress = newProgress;
  Q_EMIT progressChanged(progress);
}

void PAGXViewModel::notifyPlaybackFinished(uint64_t generation) {
  QMetaObject::invokeMethod(this, "handlePlaybackFinished", Qt::QueuedConnection,
                            Q_ARG(quint64, static_cast<quint64>(generation)));
}

void PAGXViewModel::handlePlaybackFinished(quint64 generation) {
  // Ignore a finish event from an earlier playback: if the user restarted playback after the
  // event was queued, playbackGeneration has advanced and this notification is stale.
  if (generation != playbackGeneration.load()) {
    return;
  }
  setIsPlaying(false);
}

PAGXViewModel::RenderState PAGXViewModel::getRenderState() {
  std::lock_guard<std::mutex> lock(renderMutex);
  return {scene,    defaultTimeline,          pagxWidth, pagxHeight, isPlaying_.load(),
          progress, playbackGeneration.load()};
}

bool PAGXViewModel::hasContent() {
  std::lock_guard<std::mutex> lock(renderMutex);
  return scene != nullptr;
}

void PAGXViewModel::setIsPlaying(bool isPlaying) {
  if (!hasAnimation()) {
    isPlaying = false;
  }
  if (isPlaying_ == isPlaying) {
    return;
  }
  if (isPlaying) {
    playbackGeneration++;
  }
  isPlaying_ = isPlaying;
  Q_EMIT isPlayingChanged(isPlaying);
  // Request a render on every transition, not just on play: the render thread needs one flush to
  // observe the state change (re-arm or seek the timeline and update its transition tracker).
  needsRender = true;
  Q_EMIT requestFlush();
}

void PAGXViewModel::setProgress(double newProgress) {
  if (std::abs(progress - newProgress) < 1e-9) {
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
    Q_EMIT filePathChanged("");
    return false;
  }

  auto document = pagx::PAGXImporter::FromXML(byteData->data(), byteData->length());
  if (document == nullptr) {
    Q_EMIT filePathChanged("");
    return false;
  }
  document->applyLayout();

  auto newScene = pagx::PAGScene::Make(document);
  if (newScene == nullptr) {
    {
      std::lock_guard<std::mutex> lock(renderMutex);
      clearContent();
      // Reset playback state so a prior playing session does not leave the controls stuck; with a
      // null timeline this stops playback, zeroes the frame counters, and bumps the generation.
      updateAnimationState();
    }
    Q_EMIT filePathChanged("");
    Q_EMIT pagxDocumentChanged(nullptr);
    emitContentStateReset();
    return false;
  }

  auto xmlString = QString::fromUtf8(reinterpret_cast<const char*>(byteData->data()),
                                     static_cast<qsizetype>(byteData->length()));

  {
    std::lock_guard<std::mutex> lock(renderMutex);
    clearContent();
    currentFilePath = strPath;
    pagxDocument = document;
    scene = newScene;
    defaultTimeline = scene->getDefaultTimeline();
    pagxWidth = static_cast<int>(document->width);
    pagxHeight = static_cast<int>(document->height);
    updateAnimationState();
    needsRender = true;
  }
  Q_EMIT filePathChanged(QString::fromLocal8Bit(strPath.data()));
  Q_EMIT widthChanged(pagxWidth);
  Q_EMIT heightChanged(pagxHeight);
  emitContentStateReset();
  Q_EMIT preferredSizeChanged();
  Q_EMIT editableTextLayerCountChanged(0);
  Q_EMIT editableImageLayerCountChanged(0);
  Q_EMIT contentSizeChanged();

  // pagxDocumentChanged is connected with Qt::QueuedConnection, so tree building
  // happens asynchronously and won't block the render.
  Q_EMIT pagxDocumentChanged(pagxDocument);

  // Save XML content for deferred update. The actual XmlLinesModel::setContent()
  // will be called from onRenderCompleted() after the first render finishes.
  // This avoids race conditions between ListView updates and texture presentation.
  pendingXmlContent = xmlString;

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
  scene = nullptr;
  defaultTimeline = nullptr;
}

void PAGXViewModel::emitContentStateReset() {
  Q_EMIT progressChanged(0.0);
  Q_EMIT isPlayingChanged(hasAnimation());
  Q_EMIT totalFrameChanged();
  Q_EMIT hasAnimationChanged(hasAnimation());
}

void PAGXViewModel::updateAnimationState() {
  if (defaultTimeline != nullptr && defaultTimeline->duration() > 0) {
    auto durationUs = defaultTimeline->duration();
    auto rate = defaultTimeline->frameRate();
    totalFrames =
        static_cast<int64_t>(std::round(static_cast<double>(durationUs) * rate / 1000000.0));
    if (totalFrames < 1) {
      totalFrames = 1;
    }
    frameRate = rate;
    progressPerFrame = 1.0 / static_cast<double>(totalFrames);
  } else {
    totalFrames = 1;
    frameRate = 0.0f;
    progressPerFrame = 1.0;
  }
  progress = 0.0;
  // Bump the generation on every content rebuild so any progress or finish notification still
  // queued from the previously loaded content is treated as stale and dropped on the main thread,
  // even when the new content does not animate.
  playbackGeneration++;
  isPlaying_ = hasAnimation();
}

XmlLinesModel* PAGXViewModel::linesModel() const {
  return xmlLinesModel;
}

QString PAGXViewModel::applyXmlChanges(const QString& newXml) {
  auto xmlBytes = newXml.toUtf8();
  auto document = pagx::PAGXImporter::FromXML(reinterpret_cast<const uint8_t*>(xmlBytes.data()),
                                              static_cast<size_t>(xmlBytes.length()));
  if (document == nullptr) {
    return tr("Failed to parse XML: invalid syntax or structure");
  }
  document->applyLayout();

  auto newScene = pagx::PAGScene::Make(document);
  if (newScene == nullptr) {
    return tr("Failed to build PAGScene from XML document");
  }

  {
    std::lock_guard<std::mutex> lock(renderMutex);
    pagxDocument = document;
    pagxWidth = static_cast<int>(document->width);
    pagxHeight = static_cast<int>(document->height);
    scene = newScene;
    defaultTimeline = scene->getDefaultTimeline();
    updateAnimationState();
    needsRender = true;
  }

  Q_EMIT widthChanged(pagxWidth);
  Q_EMIT heightChanged(pagxHeight);
  emitContentStateReset();
  Q_EMIT contentSizeChanged();
  Q_EMIT pagxDocumentChanged(pagxDocument);
  Q_EMIT requestFlush();

  return {};  // Empty string means success
}

QString PAGXViewModel::saveXmlToFile(const QString& xml) {
  if (currentFilePath.empty()) {
    return tr("No file path specified");
  }
  QFile file(QString::fromLocal8Bit(currentFilePath.data()));
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return tr("Failed to open file for writing: %1").arg(file.errorString());
  }
  auto xmlBytes = xml.toUtf8();
  qint64 bytesWritten = file.write(xmlBytes);
  if (bytesWritten != xmlBytes.length()) {
    return tr("Failed to write all data to file");
  }
  file.close();

  return {};  // Empty string means success
}

void PAGXViewModel::onRenderCompleted() {
  if (pendingXmlContent.isEmpty()) {
    return;
  }
  auto xmlContent = std::move(pendingXmlContent);
  pendingXmlContent.clear();
  xmlLinesModel->setContent(xmlContent);
}

}  // namespace pag
