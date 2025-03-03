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
#ifndef PAGPANELEXPORTERDIALOG_H
#define PAGPANELEXPORTERDIALOG_H

#include <QDialog>
#include <QtCore/QEventLoop>
#include <QtCore/QTranslator>
#include <QtQml/QQmlApplicationEngine>
#include <QtQuick/QQuickWindow>
#include <QtWidgets/QListView>
#include <QtWidgets/QTableView>
#include "AEResource.h"
#include "src/ui/qt/EnvConfig.h"
#include "src/ui/qt/ErrorList/ErrorListModel.h"
#include "src/ui/qt/ExportSettingDialog/ExportSettingDialog.h"
#include "src/ui/qt/PAGPanelExporterDialog/PanelExporterDataModel.h"
#include "src/utils/ExportProgressItem.h"
class PAGPanelExporterDialog final : public QObject {
  Q_OBJECT

 public:
  explicit PAGPanelExporterDialog(QWidget* parent = nullptr);
  ~PAGPanelExporterDialog() override;
  void showMainPage();
  void hide();
  void translate();
  Q_INVOKABLE void show();
  Q_INVOKABLE void resetData();
  Q_INVOKABLE void refreshErrorListView();
  Q_INVOKABLE void exportFiles();
  Q_INVOKABLE void exit();
  Q_INVOKABLE bool isAudioExport();
  Q_INVOKABLE void onExportAudioChange(bool checked);
  Q_INVOKABLE void searchCompositionsByName(QString name);
  Q_INVOKABLE void resetAfterNoSearch();
  Q_INVOKABLE void setAllChecked(bool checked);

 public Q_SLOTS:
  void goToSettingDialog(std::shared_ptr<AEResource> aeResource);
  void goToPreviewDialog(std::shared_ptr<AEResource> aeResource);
  void onCancelBtnClick();
  void onExportBtnClick();
  void handleCompositionCheckBoxChange();
  void onTitleCheckBoxStateChange(int state);
  void onSavePathItemClick(const QModelIndex& index);
  void refreshDialog();

 private:
  void initLayerTableView();
  void initErrorListView();
  void registerConnect();
  void resetToLastSelected(
      const std::shared_ptr<QList<std::shared_ptr<ConfigLayerData>>>& layerDataList) const;
  void showExportSettingDialog(AEGP_ItemH& currentAEItem);

  QApplication* app = nullptr;
  QQmlApplicationEngine* engine = nullptr;
  QQuickWindow* window = nullptr;
  std::shared_ptr<AEResource> aeResource;
  ExportConfigLayerModel* layerModel;
  ErrorListModel* errorListModel;
  QTranslator* translator;
  bool isExportAudio = false;
  ExportSettingDialog* settingDialog = nullptr;
};

#endif  //PAGPANELEXPORTERDIALOG_H
