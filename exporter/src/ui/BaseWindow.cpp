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

#include "BaseWindow.h"
#include <QApplication>
#include <QTranslator>
#include "config/ConfigFile.h"
#include "platform/PlatformHelper.h"

namespace exporter {

BaseWindow::BaseWindow(QApplication* app, QObject* parent) : QObject(parent), app(app) {
  engine = std::make_unique<QQmlApplicationEngine>(app);
  switchLanguage();
  engine->addImportPath(GetQmlPath().data());
}

void BaseWindow::show() {
  if (window == nullptr) {
    return;
  }
  AttachWindowToAEMainWindow(window);
  // The QML windows are created with "visible: true", so show() is a no-op once the window is
  // on screen. A repeated menu click must bring the window back to the front instead of doing
  // nothing when it is covered by the host application or minimized by the user.
  if (window->windowStates() & Qt::WindowMinimized) {
    window->setWindowStates(window->windowStates() & ~Qt::WindowMinimized);
  }
  window->show();
  window->raise();
  window->requestActivate();
}

void BaseWindow::switchLanguage() const {
  engine->retranslate();
}

bool BaseWindow::isWaitToDestory() const {
  return waitToDestory;
}

void BaseWindow::onWindowClosing() {
  if (window != nullptr) {
    window->hide();
  }
  waitToDestory = true;
}

}  // namespace exporter
