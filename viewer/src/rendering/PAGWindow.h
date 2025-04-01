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
#include <QString>
#include "PAGView.h"
#include "PAGWindowHelper.h"

namespace pag {
class PAGWindow : public QObject {
  Q_OBJECT
 public:
  explicit PAGWindow(QObject* parent = nullptr);
  ~PAGWindow() override;

  Q_SIGNAL void destroyWindow(PAGWindow* window);

  Q_SLOT void openFile(QString path);
  Q_SLOT void onPAGViewerDestroyed();

  auto open() -> void;
  auto getFilePath() -> QString;

  static QList<PAGWindow*> AllWindows;

 private:
  QString filePath;
  QQuickWindow* window = nullptr;
  PAGView* pagView = nullptr;
  QThread* renderThread = nullptr;
  PAGWindowHelper* windowHelper = nullptr;
  QQmlApplicationEngine* engine = nullptr;
};

}  // namespace pag
