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

double ProgressModel::getTotalProgress() const {
  if (totalProgress == 0) {
    return 1.0;
  }
  return totalProgress;
}

double ProgressModel::getCurrentProgress() const {
  return currentProgress;
}

void ProgressModel::setExportStatus(ExportStatus status) {
  if (status == ExportStatus::Success) {
    currentProgress = totalProgress;
    Q_EMIT currentProgressChanged(currentProgress);
    Q_EMIT exportFinished();
  } else if (status == ExportStatus::Error) {
    Q_EMIT exportFailed();
  }
  this->status = status;
  Q_EMIT exportStatusChanged(static_cast<int>(status));
}

void ProgressModel::setCurrentProgress(double currentProgress) {
  this->currentProgress = currentProgress;
  Q_EMIT currentProgressChanged(currentProgress);
}

void ProgressModel::setTotalProgress(double totalProgress) {
  this->totalProgress = totalProgress;
  Q_EMIT totalProgressChanged(this->totalProgress);

  if (currentProgress > totalProgress) {
    currentProgress = totalProgress;
    Q_EMIT currentProgressChanged(currentProgress);
  }
}

void ProgressModel::addProgress(double value) {
  currentProgress += value;
  if (currentProgress >= totalProgress) {
    currentProgress = totalProgress;
  }
  QApplication::processEvents();
  Q_EMIT currentProgressChanged(currentProgress);
}

void ProgressModel::addTotalProgress(double value) {
  totalProgress += value;
  Q_EMIT totalProgressChanged(totalProgress);
}

}  // namespace exporter
