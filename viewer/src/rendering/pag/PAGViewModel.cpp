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

#include "rendering/pag/PAGViewModel.h"
#include <QQuickWindow>
#include "rendering/pag/PAGView.h"
#include "pag/file.h"
#include "tgfx/core/Clock.h"
#include "version.h"

namespace pag {

constexpr int64_t MaxAudioLeadThreshold = 25000;
constexpr int64_t MinAudioLagThreshold = -100000;

static void reportPAGFileInfo(const std::shared_ptr<PAGFile>& pagFile, size_t length) {
  QVariantMap map;
  map["Event"] = "OPEN_PAG";
  map["FileSize"] = QString::number(length);
  map["PAGLayerCount"] = QString::number(pagFile->numChildren());
  map["VideoCompositionCount"] = QString::number(pagFile->numVideos());
  map["PAGTextLayerCount"] = QString::number(pagFile->numTexts());
  map["PAGImageLayerCount"] = QString::number(pagFile->numImages());
  qDebug() << map;
}

PAGViewModel::PAGViewModel(PAGView* view, QObject* parent) : ContentViewModel(parent), view(view) {
  pagPlayer = std::make_unique<PAGPlayer>();
  audioPlayer = std::make_unique<PAGAudioPlayer>();
  connect(audioPlayer.get(), &PAGAudioPlayer::audioTimeChanged, this,
          &PAGViewModel::onAudioTimeChanged, Qt::QueuedConnection);
}

int PAGViewModel::getWidth() const {
  if (pagFile == nullptr) {
    return 0;
  }
  return pagFile->width();
}

int PAGViewModel::getHeight() const {
  if (pagFile == nullptr) {
    return 0;
  }
  return pagFile->height();
}

bool PAGViewModel::hasAnimation() const {
  return true;
}

int PAGViewModel::getEditableTextLayerCount() const {
  return editableTextLayerCount;
}

int PAGViewModel::getEditableImageLayerCount() const {
  return editableImageLayerCount;
}

QString PAGViewModel::getTotalFrame() const {
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

QString PAGViewModel::getCurrentFrame() const {
  int64_t totalFrames = getTotalFrame().toLongLong();
  auto currentFrame =
      static_cast<int64_t>(std::floor(getProgress() * static_cast<double>(totalFrames)));
  return QString::number(currentFrame);
}

bool PAGViewModel::isPlaying() const {
  return isPlaying_;
}

bool PAGViewModel::getShowVideoFrames() const {
  if (pagPlayer == nullptr) {
    return false;
  }
  return pagPlayer->videoEnabled();
}

QString PAGViewModel::getDuration() const {
  if (pagPlayer == nullptr) {
    return "0";
  }
  return QString::number(pagPlayer->duration() / 1000);
}

double PAGViewModel::getProgress() const {
  return progress;
}

QString PAGViewModel::getFilePath() const {
  if (pagFile == nullptr) {
    return "";
  }
  return QString::fromLocal8Bit(pagFile->path().data());
}

QString PAGViewModel::getDisplayedTime() const {
  int64_t displayedTime =
      static_cast<int64_t>(std::round(getProgress() * getDuration().toLongLong() / 1000.0));
  int64_t displayedSeconds = displayedTime % 60;
  int64_t displayedMinutes = (displayedTime / 60) % 60;
  return QString("%1:%2")
      .arg(displayedMinutes, 2, 10, QChar('0'))
      .arg(displayedSeconds, 2, 10, QChar('0'));
}

QColor PAGViewModel::getBackgroundColor() const {
  if (pagFile == nullptr) {
    return QColorConstants::Black;
  }
  auto color = pagFile->getFile()->getRootLayer()->composition->backgroundColor;
  return QColor::fromRgb((int32_t)color.red, (int32_t)color.green, (int32_t)color.blue);
}

QSizeF PAGViewModel::getPreferredSize() const {
  if (view == nullptr) {
    return {0, 0};
  }
  auto quickWindow = view->window();
  int pagWidth = getWidth();
  int pagHeight = getHeight();
  if (quickWindow == nullptr || pagWidth == 0 || pagHeight == 0) {
    return {0, 0};
  }
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

void PAGViewModel::setIsPlaying(bool isPlaying) {
  if (this->isPlaying_ == isPlaying) {
    return;
  }
  audioPlayer->setIsPlaying(isPlaying);
  this->isPlaying_ = isPlaying;
  Q_EMIT isPlayingChanged(isPlaying);
  if (isPlaying) {
    lastPlayTime = tgfx::Clock::Now();
    if (view != nullptr) {
      view->triggerFlush();
    }
  }
}

void PAGViewModel::setShowVideoFrames(bool isShow) {
  if (pagPlayer == nullptr) {
    return;
  }
  pagPlayer->setVideoEnabled(isShow);
}

void PAGViewModel::setProgress(double progress) {
  if (this->progress == progress) {
    return;
  }
  setProgressInternal(progress, true);
}

bool PAGViewModel::loadFile(const QString& filePath) {
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
  audioPlayer->setComposition(pagFile);
  if (view != nullptr) {
    view->setSize(getPreferredSize());
  }
  view->sizeChanged = true;
  progressPerFrame = 1.0 / (pagFile->frameRate() * pagFile->duration() / 1000000);
  Q_EMIT fileChanged(pagFile->getFile());
  Q_EMIT filePathChanged(QString::fromLocal8Bit(strPath.data()));
  Q_EMIT pagFileChanged(pagFile);
  Q_EMIT widthChanged(getWidth());
  Q_EMIT heightChanged(getHeight());
  Q_EMIT totalFrameChanged();
  Q_EMIT preferredSizeChanged();
  audioPlayer->setVolume(1.0f);
  setProgressInternal(0, true);
  setIsPlaying(true);

  editableTextLayerCount = static_cast<int>(pagFile->getEditableIndices(LayerType::Text).size());
  editableImageLayerCount = static_cast<int>(pagFile->getEditableIndices(LayerType::Image).size());
  Q_EMIT editableTextLayerCountChanged(editableTextLayerCount);
  Q_EMIT editableImageLayerCountChanged(editableImageLayerCount);

  reportPAGFileInfo(pagFile, byteData->length());

  return true;
}

void PAGViewModel::firstFrame() {
  setIsPlaying(false);
  setProgress(0);
}

void PAGViewModel::lastFrame() {
  setIsPlaying(false);
  setProgress(1);
}

void PAGViewModel::nextFrame() {
  setIsPlaying(false);
  auto newProgress = this->progress + progressPerFrame;
  if (newProgress > 1) {
    newProgress = 0.0;
  }
  setProgress(newProgress);
}

void PAGViewModel::previousFrame() {
  setIsPlaying(false);
  auto newProgress = this->progress - progressPerFrame;
  if (newProgress < 0) {
    newProgress = 1.0;
  }
  setProgress(newProgress);
}

void PAGViewModel::onAudioTimeChanged(int64_t audioTime) {
  auto timeNow = tgfx::Clock::Now();
  auto displayTime = timeNow - lastPlayTime;
  auto duration = static_cast<double>(pagPlayer->duration());
  auto currentDisplayTime = static_cast<int64_t>(progress * duration) + displayTime;
  if (audioTime == 0 || (audioTime - currentDisplayTime > MaxAudioLeadThreshold)) {
    lastPlayTime = timeNow;
    setProgressInternal(static_cast<double>(audioTime) / duration, false);
  } else if (audioTime - currentDisplayTime < MinAudioLagThreshold) {
    lastPlayTime = timeNow;
  }
}

bool PAGViewModel::hasAudio() const {
  return audioPlayer != nullptr && !audioPlayer->isEmpty();
}

void PAGViewModel::setProgressInternal(double progress, bool isAudioSeek) {
  if (isAudioSeek) {
    audioPlayer->setProgress(progress);
  }
  pagPlayer->setProgress(progress);
  this->progress = progress;
  Q_EMIT progressChanged(progress);
  if (view != nullptr) {
    view->triggerFlush();
  }
}

}  // namespace pag
