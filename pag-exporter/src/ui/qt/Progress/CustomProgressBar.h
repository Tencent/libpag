/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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
#ifndef CUSTOMPROGRESSBAR_H
#define CUSTOMPROGRESSBAR_H
#include <QtCore/QObject>
#include <QtWidgets/QDialog>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QGridLayout>
#include "src/exports/PAGDataTypes.h"

#include "ui_QWidgetProgress.h"
#include "ProgressBase.h"

class CustomProgressBar : public QDialog, public ProgressBase{
  Q_OBJECT
 public:
  explicit CustomProgressBar(QWidget* parent = nullptr);
  explicit CustomProgressBar(pagexporter::Context* context, QWidget* parent = nullptr);
  ~CustomProgressBar();

  void setProgress(double progressValue) override;
  void setProgressTips(const QString& tip) override;
  void setProgressTitle(const QString& title) override;

protected:
  void closeEvent(QCloseEvent*) override;

private:
  void init(pagexporter::Context* context);
  Ui::QWidgetProgress* ui;
  double progress = 0.0;
  int totalFrames = 1;
  int currentFrames = 0;
};

#endif //CUSTOMPROGRESSBAR_H
