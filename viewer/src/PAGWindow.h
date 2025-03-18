/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QString>
#include "PAGView.h"

class PAGWindow : public QObject {
  Q_OBJECT
 public:
  PAGWindow(QObject* parent = nullptr);
  ~PAGWindow();

  static QList<PAGWindow*> AllWindows;

  void Open();
  QString getFilePath() {
    return filePath;
  }

  Q_SLOT void OpenFile(QString path);
  Q_SLOT void onPAGViewerDestroyed();
  Q_SIGNAL void DestroyWindow(PAGWindow* window);

 private:
  QString filePath = "";
  QQmlApplicationEngine* engine = nullptr;
  pag::PAGView* pagView = nullptr;
  QQuickWindow* window = nullptr;
  QThread* renderThread = nullptr;
};
