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
#ifndef PREVIEWPROGRESS_H
#define PREVIEWPROGRESS_H
#include <QtCore/QObject>
#include <QtWidgets/QProgressBar>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickWindow>
#include <QtWidgets/QApplication>
#include "src/exports/PAGDataTypes.h"
#include "ProgressBase.h"
#include <QtCore/QTranslator>

class PreviewProgress : public QObject, public ProgressBase{
  Q_OBJECT
 public:
  explicit PreviewProgress(QWidget* parent = nullptr);
  explicit PreviewProgress(pagexporter::Context* context, QWidget* parent = nullptr);
  ~PreviewProgress();

  Q_INVOKABLE void windowClose();

  void setProgress(double progressValue) override;
  void setProgressTips(const QString& tip) override;
  void setProgressTitle(const QString& title) override;
  void translate();

private:
  void init(pagexporter::Context* context);

  QQmlApplicationEngine* engine = nullptr;
  QQuickWindow* window = nullptr;
  QTranslator* translator;
  double progress = 0.0;
  int totalFrames = 1;
  int currentFrames = 0;
};

#endif //PREVIEWPROGRESS_H
