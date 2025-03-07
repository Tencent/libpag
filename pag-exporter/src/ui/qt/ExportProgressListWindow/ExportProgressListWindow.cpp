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
#include "ExportProgressListWindow.h"
#include <QtCore/QDebug>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickWindow>
#include <QtWidgets/QFileDialog>
#include <utility>
#include "src/exports/PAGExporter/PAGExporter.h"
#include "src/ui/qt/EnvConfig.h"
#include "src/ui/qt/TextEdit/TextEdit.h"
#include "src/utils/Panels/AECompositionPanel.h"

ExportProgressListWindow::ExportProgressListWindow(
    std::vector<std::shared_ptr<ExportProgressItem>> progressItems, QWidget* parent)
    : progressItems(std::move(progressItems)) {
  engine = new QQmlApplicationEngine(CurrentUseQtApp());

  QQmlContext* ctx = engine->rootContext();
  ctx->setContextProperty("exportProgressListWindow", this);

  initProgressListView();
  registerConnect();
}

ExportProgressListWindow::~ExportProgressListWindow() {
  if (progressListModel) {
    delete progressListModel;
    progressListModel = nullptr;
  }
}

int ExportProgressListWindow::progressCount() const {
  return progressListModel->rowCount();
}

void ExportProgressListWindow::setProgress(const int index, const float progressValue) const {
  progressListModel->setData(index, progressValue, "progressValue");
  QApplication::processEvents();
}

void ExportProgressListWindow::exportSuccess(int index) {
  progressListModel->setData(index, STATUS_SUCCESS, "statusCode");
  Q_EMIT successProgressCountChanged();
}

void ExportProgressListWindow::exportWrong(const int index, const std::string& errorInfo) const {
  progressListModel->setData(index, STATUS_WRONG, "statusCode");
  progressListModel->setData(index, errorInfo.c_str(), "errorInfo");
}

void ExportProgressListWindow::exportAllProgressFinish() {
  exportFinish = true;
  Q_EMIT allProgressFinished();
}

void ExportProgressListWindow::initProgressListView() {
  progressListModel = new ProgressListModel(progressItems);
  QQmlContext* ctx = engine->rootContext();
  ctx->setContextProperty("progressListModel", progressListModel);
}

void ExportProgressListWindow::registerConnect() {
  connect(progressListModel, &ProgressListModel::previewIconClicked, this,
          &ExportProgressListWindow::goToPreviewDialog);
}

void ExportProgressListWindow::goToPreviewDialog(const QModelIndex& index) {
  std::string savePath = progressListModel->getProgressItem(index)->saveFilePath;
  // PreviewPagFile(savePath);
}

void ExportProgressListWindow::onCancelBtnClick() {
  exit();
}

void ExportProgressListWindow::exit() {
  earlyExit = true;
  if (currentExportProgress != nullptr && currentExportProgress->context != nullptr) {
    currentExportProgress->context->bEarlyExit = true;
  }
  hide();
}

void ExportProgressListWindow::showProgressListWindow() {
  engine->load(QUrl(QStringLiteral("qrc:/qml/ExportProgressWindow.qml")));
  window = static_cast<QQuickWindow*>(engine->rootObjects().at(0));

  Qt::WindowFlags flags = window->flags();
  flags |= Qt::WindowCloseButtonHint;
  flags |= Qt::WindowStaysOnTopHint;
  window->setFlags(flags);

  window->setTitle(tr("PAG生成中"));
  window->setTextRenderType(QQuickWindow::TextRenderType::NativeTextRendering);
#ifdef ENABLE_OLD_API
  window->setPersistentOpenGLContext(true);
#else
  window->setPersistentGraphics(true);
#endif
  window->setPersistentSceneGraph(true);
  window->show();
}

void ExportProgressListWindow::startExport() {
  if (progressItems.empty()) {
    return;
  }

  const auto size = progressItems.size();
  for (int i = 0; i < size; i++) {
    if (earlyExit) {
      return;
    }
    std::vector<pagexporter::AlertInfo> alertInfos;
    const std::shared_ptr<ExportProgressItem> item = progressItems.at(i);
    currentExportProgress = new ProgressItemWrapper(this, i);
    qDebug() << "begin export pag:" << QString(item->pagName.c_str());
    bool success = PAGExporter::ExportFile(item->aeResource->itemH, item->saveFilePath,
                                           item->isExportAudio, false, tr("PAG导出进度"),
                                           tr("正在导出PAG"), currentExportProgress, &alertInfos);
    currentExportProgress = nullptr;
    if (success) {
      exportSuccess(i);
    } else {
      std::string errorTip;
      for (const pagexporter::AlertInfo& alertInfo : alertInfos) {
        if (alertInfo.isError) {
          errorTip = alertInfo.info + ":" + alertInfo.suggest;
          break;
        }
      }
      exportWrong(i, errorTip);
    }
  }
  exportAllProgressFinish();
}

void ExportProgressListWindow::show() const {
  if (window) {
    window->show();
  }
}

void ExportProgressListWindow::hide() const {
  if (window) {
    window->hide();
  }
}

ProgressItemWrapper::ProgressItemWrapper(ExportProgressListWindow* progressWindow, int index)
    : progressWindow(progressWindow), index(index) {
}

ProgressItemWrapper::~ProgressItemWrapper() {
  progressWindow = nullptr;
}

void ProgressItemWrapper::setProgress(double progressValue) {
  if (progressValue > 100) {
    qDebug() << "ProgressItemWrapper progressValue：" << progressValue;
    progressValue = 100;
  }
  if (progressWindow != nullptr) {
    progressWindow->setProgress(index, progressValue / 100);
  }
}

void ProgressItemWrapper::setProgressTips(const QString& tip) {
}

void ProgressItemWrapper::setProgressTitle(const QString& title) {
}
