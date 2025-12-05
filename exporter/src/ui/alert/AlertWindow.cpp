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

#include "AlertWindow.h"
#include <QApplication>
#include <QQmlContext>
#include <QThread>

namespace exporter {

AlertWindow::AlertWindow(QApplication* app, QObject* parent) : BaseWindow(app, parent) {
}

bool AlertWindow::showWarnings(const std::vector<AlertInfo>& infos) {
  init(infos);
  engine->load(QUrl(QStringLiteral("qrc:/qml/AlertWarning.qml")));
  window = qobject_cast<QQuickWindow*>(engine->rootObjects().first());
  window->setPersistentGraphics(true);
  window->setPersistentSceneGraph(true);
  QQuickWindow::setTextRenderType(QQuickWindow::TextRenderType::NativeTextRendering);
  window->show();
  wait();

  return continue_;
}

bool AlertWindow::showErrors(const std::vector<AlertInfo>& infos, const QString& errorMessage) {
  init(infos);
  if (!errorMessage.isEmpty()) {
    alertInfoModel->setErrorMessage(errorMessage);
  }
  engine->load(QUrl(QStringLiteral("qrc:/qml/AlertError.qml")));
  window = qobject_cast<QQuickWindow*>(engine->rootObjects().first());
  window->setPersistentGraphics(true);
  window->setPersistentSceneGraph(true);
  QQuickWindow::setTextRenderType(QQuickWindow::TextRenderType::NativeTextRendering);
  window->show();
  wait();

  return false;
}

void AlertWindow::onWindowClosing() {
  cancel = true;
  BaseWindow::onWindowClosing();
}

void AlertWindow::continueExport() {
  continue_ = true;
}

void AlertWindow::cancelAndModify() {
  cancel = true;
}

void AlertWindow::init(const std::vector<AlertInfo>& infos) {
  alertInfoModel = std::make_unique<AlertInfoModel>();
  alertInfoModel->setAlertInfos(infos);

  auto context = engine->rootContext();
  context->setContextProperty("alertWindow", this);
  context->setContextProperty("alertInfoModel", alertInfoModel.get());
}

void AlertWindow::wait() const {
  while (!continue_ && !cancel) {
    QApplication::processEvents();
  }
}

}  // namespace exporter
