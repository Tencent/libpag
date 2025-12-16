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

#include "ExportWindow.h"
#include <QFileDialog>
#include <QQmlContext>
#include <QThread>
#include "export/PAGExport.h"
#include "platform//PlatformHelper.h"
#include "ui/ProgressModel.h"
#include "utils/FileHelper.h"

namespace exporter {

ExportWindow::ExportWindow(QApplication* app, const std::string& outputPath, QObject* parent)
    : BaseWindow(app, parent), showAlertInfo(outputPath.empty()), outputPath(outputPath) {
  init();
}

void ExportWindow::show() {
  if (pagExport == nullptr) {
    return;
  }
  BaseWindow::show();

  bool result = pagExport->exportFile();
  if (result) {
    pagExport->session->progressModel.setExportStatus(ProgressModel::ExportStatus::Success);
  } else {
    pagExport->session->progressModel.setExportStatus(ProgressModel::ExportStatus::Error);
  }
  auto context = engine->rootContext();
  context->setContextProperty("progressModel", nullptr);
  pagExport.reset();
  if (result) {
    OpenPAGFile(outputPath);
  }
}

void ExportWindow::onWindowClosing() {
  if (pagExport != nullptr && pagExport->session != nullptr) {
    pagExport->session->stopExport = true;
  }
  BaseWindow::onWindowClosing();
}

std::string ExportWindow::getOutputPath() {
  if (itemHandle == nullptr) {
    return "";
  }
  std::string itemName = GetItemName(itemHandle) + ".pag";

  QDir dir(GetProjectPath());
  QString defaultPath = dir.filePath(itemName.data());
  QFileDialog dialog(QApplication::topLevelWidgets().value(0), QObject::tr("Select Storage Path"),
                     defaultPath);
  dialog.setAcceptMode(QFileDialog::AcceptSave);
  dialog.setDefaultSuffix("pag");

  if (dialog.exec() == QDialog::Accepted) {
    QStringList selectedFiles = dialog.selectedFiles();
    if (!selectedFiles.isEmpty()) {
      return selectedFiles.first().toStdString();
    }
  }

  return "";
}

void ExportWindow::init() {
  if (QThread::currentThread() != app->thread()) {
    qCritical() << "Must call init() in main thread";
    return;
  }

  itemHandle = GetActiveCompositionItem();
  if (itemHandle == nullptr) {
    return;
  }

  if (outputPath.empty()) {
    outputPath = getOutputPath();
    if (outputPath.empty()) {
      return;
    }
    qDebug() << "Select outputPath: " << outputPath.data();
  }

  PAGExportConfigParam configParam = {};
  configParam.exportAudio = true;
  configParam.activeItemHandle = itemHandle;
  configParam.outputPath = outputPath;
  configParam.showAlertInfo = showAlertInfo;
  pagExport = std::make_unique<PAGExport>(configParam);

  auto context = engine->rootContext();
  QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
  context->setContextProperty("exportWindow", this);
  QQmlEngine::setObjectOwnership(&pagExport->session->progressModel, QQmlEngine::CppOwnership);
  context->setContextProperty("progressModel", &pagExport->session->progressModel);

  engine->load(QUrl(QStringLiteral("qrc:/qml/ExportCompositionProgress.qml")));
  window = qobject_cast<QQuickWindow*>(engine->rootObjects().first());
  window->setPersistentGraphics(true);
  window->setPersistentSceneGraph(true);
  QQuickWindow::setTextRenderType(QQuickWindow::TextRenderType::NativeTextRendering);
}

}  // namespace exporter
