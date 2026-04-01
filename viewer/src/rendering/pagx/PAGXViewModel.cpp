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
#include <QQuickWindow>
#include <cmath>
#include "pag/pag.h"
#include "pagx/PAGXImporter.h"
#include "renderer/LayerBuilder.h"

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

PAGXViewModel::RenderState PAGXViewModel::getRenderState() {
  std::lock_guard<std::mutex> lock(renderMutex);
  return {displayList, pagxContentLayer, pagxWidth, pagxHeight};
}

bool PAGXViewModel::hasContent() {
  std::lock_guard<std::mutex> lock(renderMutex);
  return displayList != nullptr;
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
    return false;
  }

  auto document = pagx::PAGXImporter::FromXML(byteData->data(), byteData->length());
  if (document == nullptr) {
    return false;
  }

  auto newContentLayer = pagx::LayerBuilder::Build(document.get());
  if (newContentLayer == nullptr) {
    Q_EMIT pagxDocumentChanged(nullptr);
    return false;
  }

  auto newDisplayList = std::make_shared<tgfx::DisplayList>();
  newDisplayList->root()->addChild(newContentLayer);

  // Store XML content for later update to XmlLinesModel
  auto xmlString = QString::fromUtf8(reinterpret_cast<const char*>(byteData->data()),
                                     static_cast<qsizetype>(byteData->length()));

  {
    std::lock_guard<std::mutex> lock(renderMutex);
    clearContent();
    currentFilePath = strPath;
    pagxDocument = document;
    pagxWidth = static_cast<int>(document->width);
    pagxHeight = static_cast<int>(document->height);
    pagxContentLayer = newContentLayer;
    updateAnimationState();
    displayList = std::move(newDisplayList);
    needsRender = true;
  }
  Q_EMIT filePathChanged(QString::fromLocal8Bit(strPath.data()));
  Q_EMIT widthChanged(pagxWidth);
  Q_EMIT heightChanged(pagxHeight);
  Q_EMIT totalFrameChanged();
  Q_EMIT hasAnimationChanged(hasAnimation());
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
  pagxContentLayer = nullptr;
  displayList = nullptr;
}

void PAGXViewModel::updateAnimationState() {
  // TODO: Read timeline properties from PAGXDocument when available.
  totalFrames = 1;
  frameRate = 0.0f;
  progress = 0.0;
  progressPerFrame = 1.0;
  isPlaying_ = false;
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

  auto newContentLayer = pagx::LayerBuilder::Build(document.get());
  if (newContentLayer == nullptr) {
    return tr("Failed to build layer from XML document");
  }

  auto newDisplayList = std::make_shared<tgfx::DisplayList>();
  newDisplayList->root()->addChild(newContentLayer);

  {
    std::lock_guard<std::mutex> lock(renderMutex);
    pagxDocument = document;
    pagxWidth = static_cast<int>(document->width);
    pagxHeight = static_cast<int>(document->height);
    pagxContentLayer = newContentLayer;
    displayList = std::move(newDisplayList);
    needsRender = true;
  }

  Q_EMIT widthChanged(pagxWidth);
  Q_EMIT heightChanged(pagxHeight);
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
