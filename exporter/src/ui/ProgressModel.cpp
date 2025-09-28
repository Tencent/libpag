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

#include "ProgressModel.h"
#include <QApplication>

namespace exporter {

ProgressModel::ProgressModel(QObject* parent) : QObject(parent) {
}

int ProgressModel::getExportStatus() const {
  return static_cast<int>(status);
}

double ProgressModel::getTotalFrame() const {
  if (totalFrame == 0) {
    return 1.0;
  }
  return totalFrame;
}

double ProgressModel::getCurrentFrame() const {
  return currentFrame;
}

void ProgressModel::setExportStatus(ExportStatus status) {
  if (status == ExportStatus::Success) {
    currentFrame = totalFrame;
    Q_EMIT currentFrameChanged(currentFrame);
    Q_EMIT exportFinished();
  } else if (status == ExportStatus::Error) {
    Q_EMIT exportFailed();
  }
  this->status = status;
  Q_EMIT exportStatusChanged(static_cast<int>(status));
}

void ProgressModel::setCurrentFrame(double currentFrame) {
  this->currentFrame = currentFrame;
  Q_EMIT currentFrameChanged(currentFrame);
}

void ProgressModel::setTotalFrame(double totalFrame) {
  this->totalFrame = totalFrame;
  Q_EMIT totalFramesChanged(this->totalFrame);

  if (currentFrame > totalFrame) {
    currentFrame = totalFrame;
    Q_EMIT currentFrameChanged(currentFrame);
  }
}

void ProgressModel::updateProgress(double value) {
  currentFrame += value;
  if (currentFrame >= totalFrame) {
    currentFrame = totalFrame;
  }
  QApplication::processEvents();
  Q_EMIT currentFrameChanged(currentFrame);
}

void ProgressModel::addTotalFrame(double value) {
  totalFrame += value;
  Q_EMIT totalFramesChanged(totalFrame);
}

}  // namespace exporter
