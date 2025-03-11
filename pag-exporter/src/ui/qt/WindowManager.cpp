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
#include <QtCore/QDir>

#include "WindowManager.h"
#include "src/ui/qt/PAGConfigDialog/PAGConfigDialog.h"
#include "src/ui/qt/PAGPanelExporterDialog/PAGPanelExporterDialog.h"
#include "src/utils/AEUtils.h"
#include "src/exports/PAGExporter/PAGExporter.h"
#include "src/ui/qt/EnvConfig.h"
#include "src/ui/qt/Progress/PreviewProgress.h"
WindowManager& WindowManager::getInstance() {
  static WindowManager instance;
  return instance;
}

WindowManager::~WindowManager() {
  if (panelExporterDialog) {
    delete panelExporterDialog;
    panelExporterDialog = nullptr;
  }
}

void WindowManager::showPanelExporterDialog() {
  if (isConfigDialogDestroy && panelExporterDialog != nullptr) {
    delete panelExporterDialog;
    panelExporterDialog = nullptr;
    isConfigDialogDestroy = false;
  }
  if (!panelExporterDialog) {
    panelExporterDialog = new PAGPanelExporterDialog();
    panelExporterDialog->showMainPage();
  } else {
    panelExporterDialog->resetData();
    panelExporterDialog->refreshErrorListView();
    panelExporterDialog->show();
  }
}

void WindowManager::exitPAGPanelExporterDialog() {
  panelExporterDialog->hide();
  isConfigDialogDestroy = true;
}

bool WindowManager::PanelExporterDialogIsActive() {
  if (panelExporterDialog) {
    return panelExporterDialog->isActive();
  }
  return false;
}

void WindowManager::showPAGConfigDialog() {
  configDialog = new PAGConfigDialog();
  configDialog->exec();
}

bool WindowManager::PAGConfigDialogIsActive() {
  if (configDialog) {
    return configDialog->isVisible();
  }
  return false;
}

void WindowManager::showExportPreviewDialog(AEGP_ItemH itemH, bool isExportAudio, bool needShowExport, bool isQWidget) {
  QDir dir(QString(AEUtils::GetFolderTempName().c_str()));  // 临时目录
  QString savePath = dir.absoluteFilePath(QString(".previewTmp.pag"));
  ProgressBase* progressBase = nullptr;
  if (!isQWidget) {
    progressBase = new PreviewProgress();
  }
  bool success = PAGExporter::ExportFile(itemH, QStringToString(savePath.toStdString()),
                                         isExportAudio, false, QObject::tr("PAG预览生成"),
                                         QObject::tr("正在生成预览"), progressBase, nullptr, true);
  if (!success) {
    qDebug() << "PAG预览生成失败";
    return;
  }

  PreviewPagFile(QStringToString(savePath.toStdString()));

}