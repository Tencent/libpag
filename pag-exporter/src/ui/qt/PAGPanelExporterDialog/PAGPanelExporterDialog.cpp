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

#include "PAGPanelExporterDialog.h"
#include <QtCore/QDir>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickWindow>
#include <QtWidgets/QFileDialog>
#include <algorithm>
#include "TextEdit/TextEdit.h"
#include "src/exports/PAGExporter/PAGExporter.h"
// #include "utils/AECompositionPanel.h"
// #include "utils/ExportProgressItem.h"
#include <QDebug>
#include <QThread>
#include <QTimer>

void onTimeout() {
  qDebug() << "Timer timeout!";
}

PAGPanelExporterDialog::PAGPanelExporterDialog(QWidget* parent) {
  int argc = 0;
  app = new QApplication(argc, nullptr);
  Q_INIT_RESOURCE(res);
  engine = new QQmlApplicationEngine();

  QQmlContext* ctx = engine->rootContext();
  ctx->setContextProperty("exportConfigDialog", this);
  TextEdit* textEdit = new TextEdit();
  ctx->setContextProperty("ExportUtils", textEdit);

  translate();
  initLayerTableView();
  //  initErrorListView();
  registConnect();
}

PAGPanelExporterDialog::~PAGPanelExporterDialog() {
}

static void covertLayerData(std::shared_ptr<AEResource>& root,
                            std::shared_ptr<QList<std::shared_ptr<ConfigLayerData>>>& layerDataList,
                            std::shared_ptr<ConfigLayerData> parent = nullptr) {
  if (root != nullptr && root->childs.empty()) {
    return;
  }
  auto aeResources = root->childs;
  for (std::shared_ptr<AEResource> resource : aeResources) {
    std::shared_ptr<ConfigLayerData> layerData = nullptr;
    if (resource->type == AEResourceType::Composition) {
      layerData = std::make_shared<ConfigLayerData>(resource);
    } else if (resource->type == AEResourceType::Folder) {
      layerData = std::make_shared<FolderData>(resource);
    } else {
      continue;
    }
    if (parent) {
      parent->addChild(layerData);
      layerData->parent = parent;
    }
    layerDataList->append(layerData);

    covertLayerData(resource, layerDataList, layerData);
  }
}

static void removeEmptyFolder(
    std::shared_ptr<QList<std::shared_ptr<ConfigLayerData>>>& layerDataList) {
  auto it = layerDataList->begin();
  while (it != layerDataList->end()) {
    std::shared_ptr<ConfigLayerData> layerData = *it;
    if (typeid(*layerData.get()) == typeid(FolderData) && !layerData->hasChild()) {
      it = layerDataList->erase(it);
    } else {
      it++;
    }
  }
}

void PAGPanelExporterDialog::resetData() {
  std::shared_ptr<QList<std::shared_ptr<ConfigLayerData>>> layerDataList =
      std::make_shared<QList<std::shared_ptr<ConfigLayerData>>>();
  auto root = AEResource::GetResourceTree();
  if (root == nullptr) {
    return;
  }
  covertLayerData(root, layerDataList);
  removeEmptyFolder(layerDataList);
  resetToLastSelected(layerDataList);
  layerModel->setLayerData(layerDataList, true);
  isExportAudio = AEResource::GetExportAudioFlag();
}

void PAGPanelExporterDialog::resetToLastSelected(
    const std::shared_ptr<QList<std::shared_ptr<ConfigLayerData>>> layerDataList) {
  if (aeResource != nullptr && !layerDataList->empty()) {
    for (auto layerData : *layerDataList) {
      if (layerData->aeResource == nullptr || layerData->aeResource->itemH == nullptr ||
          layerData->aeResource->itemH != aeResource->itemH) {
        layerData->setChecked(false);
        continue;
      }
      layerData->setChecked(true);
    }
  }
}

void PAGPanelExporterDialog::initErrorListView() {
  errorListModel = new ErrorListModel(this);
  refreshErrorListView();

  QQmlContext* ctx = engine->rootContext();
  ctx->setContextProperty("errorListModel", errorListModel);
}

void PAGPanelExporterDialog::initLayerTableView() {
  layerModel = new ExportConfigLayerModel(this);
  resetData();

  QQmlContext* ctx = engine->rootContext();
  ctx->setContextProperty("compositionModel", layerModel);
}

void PAGPanelExporterDialog::registConnect() {
  connect(layerModel, &ExportConfigLayerModel::settingIconClicked, this,
          &PAGPanelExporterDialog::goToSettingDialog);
  connect(layerModel, &ExportConfigLayerModel::previewIconClicked, this,
          &PAGPanelExporterDialog::goToPreviewDialog);
  connect(layerModel, &ExportConfigLayerModel::savePathClicked, this,
          &PAGPanelExporterDialog::onSavePathItemClick);
  connect(layerModel, &ExportConfigLayerModel::compositionCheckChanged, this,
          &PAGPanelExporterDialog::handleCompositionCheckBoxChange);
}

void PAGPanelExporterDialog::goToSettingDialog(std::shared_ptr<AEResource> aeResource) {
  hide();
  this->aeResource = aeResource;
  // ShowExportSettingDialog(aeResource->itemH);
}

void PAGPanelExporterDialog::goToPreviewDialog(std::shared_ptr<AEResource> aeResource) {
  //hide();
  this->aeResource = aeResource;
  // ShowExportPreviewDialog(aeResource->itemH, isExportAudio, true);
}

void PAGPanelExporterDialog::onCancelBtnClick() {
}

void PAGPanelExporterDialog::onExportBtnClick() {
  exportFiles();
}

void PAGPanelExporterDialog::exportFiles() {
  hide();
  // std::shared_ptr<QList<std::shared_ptr<ConfigLayerData>>> selectedLayers = layerModel->getSelectedLayer();
  // std::vector<std::shared_ptr<ExportProgressItem>> exportProgressList;
  // for (std::shared_ptr<ConfigLayerData> layerData : *selectedLayers) {
  //   if (layerData->isChecked() && layerData->aeResource && layerData->aeResource->itemH) {
  //     std::string pagFileName = layerData->name + ".pag";
  //     QDir dir(QString(layerData->aeResource->getStorePath().c_str()));
  //     QString savePath = dir.absoluteFilePath(QString(pagFileName.c_str()));
  //     std::string pagSavePath = FormatStdString(savePath.toStdString());
  //
  //     std::shared_ptr<ExportProgressItem> progressItem = std::make_shared<ExportProgressItem>(pagFileName);
  //     progressItem->saveFilePath = pagSavePath;
  //     progressItem->aeResource = layerData->aeResource;
  //     progressItem->isExportAudio = isExportAudio;
  //
  //     exportProgressList.emplace_back(progressItem);
  //   }
  // }
  // if(!exportProgressList.empty()){
  //   ShowProgressListWindow(exportProgressList);
  // }
}

void PAGPanelExporterDialog::handleCompositionCheckBoxChange() {
  refreshErrorListView();
}

void PAGPanelExporterDialog::onTitleCheckBoxStateChange(int state) {
  refreshErrorListView();
}

void PAGPanelExporterDialog::refreshErrorListView() {
  std::vector<pagexporter::AlertInfo> alertInfos;
  std::shared_ptr<QList<std::shared_ptr<ConfigLayerData>>> selectedLayer =
      layerModel->getSelectedLayer();
  for (std::shared_ptr<ConfigLayerData> layerData : *selectedLayer) {
    if (layerData->aeResource && layerData->aeResource->itemH) {
      std::vector<pagexporter::AlertInfo> alertList =
          pagexporter::AlertInfos::GetAlertList(layerData->aeResource->itemH);
      alertInfos.insert(alertInfos.end(), alertList.begin(), alertList.end());
    }
  }
  errorListModel->setAlertInfos(alertInfos);
}

void PAGPanelExporterDialog::exit() {
  app->exit();
}

bool PAGPanelExporterDialog::isAudioExport() {
  return isExportAudio;
}

void PAGPanelExporterDialog::onSavePathItemClick(const QModelIndex& index) {
  QString savePath = index.data(SAVE_PATH_ROLE).toString();
  QString selectPath = QFileDialog::getExistingDirectory(QApplication::topLevelWidgets().value(0),
                                                         tr("选择存储路径"), savePath,
                                                         QFileDialog::Option::ShowDirsOnly);
  layerModel->setData(index, selectPath, SAVE_PATH_ROLE);
}

void PAGPanelExporterDialog::onExportAudioChange(bool checked) {
  isExportAudio = checked;
  AEResource::SetExportAudioFlag(checked);
}

void PAGPanelExporterDialog::searchCompositionsByName(QString name) {
  layerModel->searchCompositionsByName(name);
  refreshErrorListView();
}

void PAGPanelExporterDialog::resetAfterNoSearch() {
  layerModel->resetAfterNoSearch();
  refreshErrorListView();
}

void PAGPanelExporterDialog::setAllChecked(bool checked) {
  layerModel->setAllChecked(checked);
  refreshErrorListView();
}

void PAGPanelExporterDialog::showMainPage() {
  QUrl qUrl = QUrl(QStringLiteral("qrc:/qml/ExportConfig.qml"));
  engine->load(qUrl);
  window = static_cast<QQuickWindow*>(engine->rootObjects().at(0));
  window->setTextRenderType(QQuickWindow::TextRenderType::NativeTextRendering);
#ifdef ENABLE_OLD_API
  window->setPersistentOpenGLContext(true);
#else
  window->setPersistentGraphics(true);
#endif
  window->setPersistentSceneGraph(true);
  window->show();
  window->setTitle(tr("导出PAG"));
  app->exec();
}

void PAGPanelExporterDialog::translate() {
  // if (translator) {
  //   app->removeTranslator(translator);
  // }
  // if (ExportUtils::isEnglish()) {
  //   translator = new QTranslator();
  //   bool result = translator->load(":/language/english.qm");
  //   bool install = app->installTranslator(translator);
  //   qDebug() << "translator result:" << result << ", install result:" << install;
  // }
  // engine->retranslate();
}

void PAGPanelExporterDialog::show() {
  if (window) {
    window->show();
  }
  translate();
}

void PAGPanelExporterDialog::hide() {
  if (window) {
    window->hide();
  }
}
