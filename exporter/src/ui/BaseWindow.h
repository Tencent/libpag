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

  // Reports whether any plugin event loop (the main exec() loop or a modal wait such as the alert
  // window) is currently pumping events, so a re-entrant entry point can avoid stacking a second,
  // nested loop. Kept here so both WindowManager and the window classes can share one source of
  // truth without depending on each other.
  static bool isEventLoopRunning();

  // RAII scope that marks an event loop as running for its lifetime, keeping isEventLoopRunning()
  // accurate while a loop (the main exec() loop or a modal QEventLoop) pumps events.
  class EventLoopScope {
   public:
    EventLoopScope();
    ~EventLoopScope();
    EventLoopScope(const EventLoopScope&) = delete;
    EventLoopScope& operator=(const EventLoopScope&) = delete;
  };

  QApplication* app = nullptr;
  QQuickWindow* window = nullptr;
  std::unique_ptr<QQmlApplicationEngine> engine = nullptr;

 private:
  static int eventLoopDepth;
  bool waitToDestory = false;
};

}  // namespace exporter
