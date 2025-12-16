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
  if (totalSteps == 0) {
    return 1.0;
  }
  return static_cast<double>(totalSteps);
}

double ProgressModel::getCurrentProgress() const {
  return static_cast<double>(finishedSteps);
}

void ProgressModel::setExportStatus(ExportStatus status) {
  if (status == ExportStatus::Success) {
    finishedSteps = totalSteps;
    Q_EMIT currentProgressChanged(static_cast<double>(finishedSteps));
    Q_EMIT exportFinished();
  } else if (status == ExportStatus::Error) {
    Q_EMIT exportFailed();
  }
  this->status = status;
  Q_EMIT exportStatusChanged(static_cast<int>(status));
}

void ProgressModel::setFinishedSteps(uint64_t value) {
  finishedSteps = value;
  Q_EMIT currentProgressChanged(static_cast<double>(finishedSteps));
}

void ProgressModel::setTotalSteps(uint64_t value) {
  totalSteps = value;
  Q_EMIT totalProgressChanged(static_cast<double>(totalSteps));

  if (finishedSteps > totalSteps) {
    finishedSteps = totalSteps;
    Q_EMIT currentProgressChanged(static_cast<double>(finishedSteps));
  }
}

void ProgressModel::addFinishedSteps(uint64_t value) {
  finishedSteps += value;
  if (finishedSteps >= totalSteps) {
    finishedSteps = totalSteps;
  }
  QApplication::processEvents();
  Q_EMIT currentProgressChanged(static_cast<double>(finishedSteps));
}

void ProgressModel::addTotalSteps(uint64_t value) {
  totalSteps += value;
  Q_EMIT totalProgressChanged(static_cast<double>(totalSteps));
}

}  // namespace exporter
