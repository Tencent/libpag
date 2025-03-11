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
#include "src/ui/qt/ExportProgressListWindow/ExportProgressListWindow.h"
#include "src/ui/qt/ExportSettingDialog/ExportSettingDialog.h"
#include "src/ui/qt/PAGPanelExporterDialog/PanelExporterDataModel.h"
#include "src/ui/qt/WindowManager.h"
#include "src/utils/ExportProgressItem.h"
class PAGPanelExporterDialog final : public QObject {
  Q_OBJECT

 public:
  explicit PAGPanelExporterDialog(QWidget* parent = nullptr);
  ~PAGPanelExporterDialog() override;
  void showMainPage();
  void hide() const;
  void translate();
  Q_INVOKABLE void show();
  Q_INVOKABLE void resetData();
  Q_INVOKABLE void refreshErrorListView() const;
  Q_INVOKABLE void exportFiles();
  Q_INVOKABLE void exit() const;
  Q_INVOKABLE bool isAudioExport() const;
  Q_INVOKABLE void onExportAudioChange(bool checked);
  Q_INVOKABLE void searchCompositionsByName(const QString& name) const;
  Q_INVOKABLE void resetAfterNoSearch() const;
  Q_INVOKABLE void setAllChecked(bool checked) const;

 public Q_SLOTS:
  void goToSettingDialog(const std::shared_ptr<AEResource>& aeResource);
  void goToPreviewDialog(const std::shared_ptr<AEResource>& aeResource);
  void onExportBtnClick();
  void handleCompositionCheckBoxChange() const;
  void onTitleCheckBoxStateChange(int state) const;
  void onSavePathItemClick(const QModelIndex& index) const;
  void refreshDialog();
  bool isActive();

 private:
  void initLayerTableView();
  void initErrorListView();
  void registerConnect();
  void resetToLastSelected(
      const std::shared_ptr<QList<std::shared_ptr<ConfigLayerData>>>& layerDataList) const;
  void showExportSettingDialog(AEGP_ItemH& currentAEItem);
  void showProgressListWindow(std::vector<std::shared_ptr<ExportProgressItem>>& progressItems);
  static inline QApplication* app = nullptr;
  QQmlApplicationEngine* engine = nullptr;
  QQuickWindow* window = nullptr;
  std::shared_ptr<AEResource> aeResource;
  ExportConfigLayerModel* layerModel = nullptr;
  ErrorListModel* errorListModel = nullptr;
  QTranslator* translator = nullptr;
  bool isExportAudio = false;
  ExportSettingDialog* settingDialog = nullptr;
  ExportProgressListWindow* exportProgressListWindow = nullptr;
  WindowManager& windowManager = WindowManager::getInstance();
};

#endif  //PAGPANELEXPORTERDIALOG_H
