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

#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include "config/ConfigParam.h"

namespace exporter {

class BaseWindow : public QObject {
  Q_OBJECT
 public:
  explicit BaseWindow(QApplication* app, QObject* parent = nullptr);

  void switchLanguage() const;

  virtual void show();
  virtual bool isWaitToDestory() const;
  Q_SLOT virtual void onWindowClosing();

  QApplication* app = nullptr;
  QQuickWindow* window = nullptr;
  std::unique_ptr<QQmlApplicationEngine> engine = nullptr;

 private:
  bool waitToDestory = false;
};

}  // namespace exporter
