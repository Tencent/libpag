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

#pragma once

#include <QObject>

namespace exporter {
class ProgressModel : public QObject {
  Q_OBJECT
 public:
  enum class ExportStatus {
    Waiting = 0,
    Success,
    Error,
  };

  explicit ProgressModel(QObject* parent = nullptr);

  Q_PROPERTY(int exportStatus READ getExportStatus NOTIFY exportStatusChanged)
  Q_PROPERTY(double totalProgress READ getTotalProgress NOTIFY totalProgressChanged)
  Q_PROPERTY(double currentProgress READ getCurrentProgress NOTIFY currentProgressChanged)

  Q_INVOKABLE int getExportStatus() const;
  Q_INVOKABLE double getTotalProgress() const;
  Q_INVOKABLE double getCurrentProgress() const;
  void setExportStatus(ExportStatus status);
  void setCurrentProgress(double currentProgress);
  void setTotalProgress(double totalProgress);
  void addProgress(double value = 1.0);
  void addTotalProgress(double value);

  Q_SIGNAL void exportStatusChanged(int exportStatus);
  Q_SIGNAL void totalProgressChanged(double totalProgress);
  Q_SIGNAL void currentProgressChanged(double currentProgress);
  Q_SIGNAL void exportFinished();
  Q_SIGNAL void exportFailed();

 private:
  ExportStatus status = ExportStatus::Waiting;
  double totalProgress = 0;
  double currentProgress = 0;
};
}  // namespace exporter
