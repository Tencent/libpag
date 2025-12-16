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

#include "CompositionsModel.h"
#include <QQmlContext>
#include <QStandardPaths>
#include <QUrl>
#include <unordered_set>
#include "ExportPanelWindow.h"
#include "export/PAGExport.h"
#include "platform/PlatformHelper.h"
#include "utils/FileHelper.h"

namespace exporter {

CompositionsModel::CompositionsModel(QObject* parent) : QAbstractListModel(parent) {
  progressListModel = std::make_unique<ProgressListModel>();
  alertInfoModel = std::make_unique<AlertInfoModel>();
}

void CompositionsModel::setAEResources(const std::vector<std::shared_ptr<AEResource>>& resources) {
  this->resources = resources;
  compositions.clear();
  for (const auto& resource : resources) {
    auto composition = std::make_shared<ExportCompositionData>();
    composition->resource = resource;
    composition->isFolder = resource->type == AEResourceType::Folder;
    compositions.push_back(composition);
  }
  updateCompositionLevel();
  updateAllSelectedNum();
  beginResetModel();
  endResetModel();
  updateAlertInfos();
}

void CompositionsModel::setQmlEngine(QQmlEngine* engine) {
  this->engine = engine;
  if (engine != nullptr) {
    auto context = engine->rootContext();
    context->setContextProperty("progressListModel", progressListModel.get());
    context->setContextProperty("alertInfoModel", alertInfoModel.get());
  }
}

bool CompositionsModel::getAllSelected() const {
  return selectedNum == allSelectedNum;
}

bool CompositionsModel::getCanExport() const {
  return selectedNum > 0;
}

bool CompositionsModel::getExportAudio() const {
  return exportAudio;
}

void CompositionsModel::setIsSelected(int index, bool isSelected, bool isAllSelected) {
  if (index < 0 || static_cast<size_t>(index) >= compositions.size()) {
    return;
  }
  if (compositions[index]->isFolder || compositions[index]->resource->isExport == isSelected) {
    return;
  }
  selectedNum += isSelected ? 1 : -1;
  compositions[index]->resource->isExport = isSelected;
  QModelIndex modelIndex = this->index(index);
  Q_EMIT dataChanged(modelIndex, modelIndex,
                     {static_cast<int>(ExportCompositionModelRoles::IsSelectedRole)});
  Q_EMIT allSelectedChanged(getAllSelected());
  Q_EMIT canExportChanged(getCanExport());
  if (!isAllSelected) {
    updateAlertInfos();
  }
}

void CompositionsModel::setIsUnfold(int index, bool isUnfold) {
  if (index < 0 || static_cast<size_t>(index) >= compositions.size()) {
    return;
  }
  const auto& composition = compositions[index];
  composition->isUnfold = isUnfold;
  std::unordered_set<A_long> idSet = {};
  idSet.emplace(composition->resource->ID);
  if (isUnfold) {
    std::vector<std::shared_ptr<ExportCompositionData>> tmpCompositions(
        compositions.begin() + index + 1, compositions.end());
    compositions.resize(index + 1);
    for (const auto& resource : resources) {
      if (resource->file.parent == nullptr ||
          idSet.find(resource->file.parent->ID) == idSet.end()) {
        continue;
      }
      auto newComposition = std::make_shared<ExportCompositionData>();
      newComposition->resource = resource;
      newComposition->isFolder = resource->type == AEResourceType::Folder;
      compositions.push_back(newComposition);
      idSet.emplace(resource->ID);
    }
    compositions.insert(compositions.end(), tmpCompositions.begin(), tmpCompositions.end());
  } else {
    auto iter = compositions.begin();
    while (iter != compositions.end()) {
      if ((*iter)->resource->file.parent == nullptr ||
          idSet.find((*iter)->resource->file.parent->ID) == idSet.end()) {
        ++iter;
        continue;
      }
      idSet.emplace((*iter)->resource->ID);
      iter = compositions.erase(iter);
    }
  }
  updateCompositionLevel();
  updateAllSelectedNum();
  beginResetModel();
  endResetModel();
}

void CompositionsModel::setSavePath(int index, const QString& savePath) {
  if (index < 0 || static_cast<size_t>(index) >= compositions.size()) {
    return;
  }
  QString path = savePath;
  if (path.startsWith("file://")) {
    path = QUrl(path).toLocalFile();
  }
  compositions[index]->resource->setSavePath(path.toStdString());
  QModelIndex modelIndex = this->index(index);
  Q_EMIT dataChanged(modelIndex, modelIndex,
                     {static_cast<int>(ExportCompositionModelRoles::SavePathRole)});
}

void CompositionsModel::setAllSelected(bool allSelected) {
  for (size_t index = 0; index < compositions.size(); index++) {
    setIsSelected(static_cast<int>(index), allSelected, true);
  }
  updateAlertInfos();
}

void CompositionsModel::setSerachText(const QString& searchText) {
  compositions.clear();
  for (const auto& resource : resources) {
    if (resource->name.find(searchText.toStdString()) == std::string::npos) {
      continue;
    }
    auto composition = std::make_shared<ExportCompositionData>();
    composition->resource = resource;
    composition->isFolder = resource->type == AEResourceType::Folder;
    compositions.push_back(composition);
  }
  updateCompositionLevel();
  updateAllSelectedNum();
  beginResetModel();
  endResetModel();
}

void CompositionsModel::setExportAudio(bool exportAudio) {
  this->exportAudio = exportAudio;
  Q_EMIT exportAudioChanged(exportAudio);
}

void CompositionsModel::exportSelectedCompositions() {
  progressListModel->clearSessions();

  auto context = engine->rootContext();
  context->setContextProperty("exportCompositionsWindow", this);

  for (const auto& resource : resources) {
    if (!resource->isExport) {
      continue;
    }
    std::string outputPath = JoinPaths(resource->savePath, resource->name + ".pag");
    PAGExportConfigParam configParam = {};
    configParam.exportAudio = exportAudio;
    configParam.activeItemHandle = resource->itemHandle;
    configParam.outputPath = outputPath;
    configParam.showAlertInfo = true;
    auto pagExport = std::make_unique<PAGExport>(configParam);
    progressListModel->addSession(pagExport->session);
    pagExports.push_back(std::move(pagExport));
  }

  qDebug() << "Selected compositions: " << pagExports.size();

  for (auto& pagExport : pagExports) {
    pagExport->session->progressModel.setExportStatus(pagExport->exportFile()
                                                           ? ProgressModel::ExportStatus::Success
                                                           : ProgressModel::ExportStatus::Error);
  }
  pagExports.clear();
  context->setContextProperty("exportCompositionsWindow", nullptr);
}

void CompositionsModel::prepareForPreview(int row) {
  if (row >= static_cast<int>(compositions.size())) {
    return;
  }
  const auto& composition = compositions[row];
  const auto& resource = composition->resource;
  std::string outputPath = JoinPaths(GetTempFolderPath(), ".previewTmp.pag");
  PAGExportConfigParam configParam = {};
  configParam.exportAudio = exportAudio;
  configParam.activeItemHandle = resource->itemHandle;
  configParam.outputPath = outputPath;
  pagExport = std::make_unique<PAGExport>(configParam);
  auto context = engine->rootContext();
  context->setContextProperty("progressModel", &pagExport->session->progressModel);
  context->setContextProperty("exportWindow", this);
}

void CompositionsModel::previewComposition(int row) {
  if (row >= static_cast<int>(compositions.size())) {
    return;
  }
  if (pagExport == nullptr) {
    return;
  }
  bool result = pagExport->exportFile();
  if (result) {
    pagExport->session->progressModel.setExportStatus(ProgressModel::ExportStatus::Success);
  } else {
    pagExport->session->progressModel.setExportStatus(ProgressModel::ExportStatus::Error);
  }
  auto context = engine->rootContext();
  context->setContextProperty("progressModel", nullptr);
  if (result) {
    OpenPAGFile(pagExport->session->outputPath);
  }
}

void CompositionsModel::updateNames() {
  Q_EMIT dataChanged(index(0, 0), index(rowCount({}) - 1, 0),
                     {static_cast<int>(ExportCompositionModelRoles::NameRole)});
}

void CompositionsModel::onWindowClosing() {
  if (pagExport != nullptr && pagExport->session != nullptr) {
    pagExport->session->stopExport = true;
  }
  for (const auto& pagExport : pagExports) {
    pagExport->session->stopExport = true;
  }
}

int CompositionsModel::rowCount(const QModelIndex&) const {
  return static_cast<int>(compositions.size());
}

int CompositionsModel::columnCount(const QModelIndex&) const {
  return 1;
}

QVariant CompositionsModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 ||
      static_cast<size_t>(index.row()) >= compositions.size()) {
    return {};
  }

  const auto& data = compositions[index.row()];
  switch (role) {
    case static_cast<int>(ExportCompositionModelRoles::NameRole): {
      return data->resource->name.c_str();
    }
    case static_cast<int>(ExportCompositionModelRoles::SavePathRole): {
      return data->resource->savePath.c_str();
    }
    case static_cast<int>(ExportCompositionModelRoles::IsSelectedRole): {
      return data->resource->isExport;
    }
    case static_cast<int>(ExportCompositionModelRoles::IsFolderRole): {
      return data->isFolder;
    }
    case static_cast<int>(ExportCompositionModelRoles::IsUnfoldRole): {
      return data->isUnfold;
    }
    case static_cast<int>(ExportCompositionModelRoles::LevelRole): {
      return data->level;
    }
    default: {
      return {};
    }
  }
}

void CompositionsModel::updateCompositionLevel() {
  for (const auto& composition : compositions) {
    int level = 0;
    auto parent = composition->resource->file.parent;
    while (parent != nullptr && parent->ID != 0) {
      ++level;
      parent = parent->file.parent;
    }
    composition->level = level;
  }
}

void CompositionsModel::updateAllSelectedNum() {
  selectedNum = 0;
  allSelectedNum = 0;
  for (const auto& composition : compositions) {
    if (!composition->isFolder) {
      if (composition->resource->isExport) {
        selectedNum++;
      }
      allSelectedNum++;
    }
  }
  Q_EMIT allSelectedChanged(getAllSelected());
}

void CompositionsModel::updateAlertInfos() {
  if (alertInfoModel == nullptr) {
    return;
  }
  AlertInfoManager::GetInstance().warningList.clear();
  for (const auto& resource : resources) {
    if (resource->type != AEResourceType::Composition) {
      continue;
    }
    if (!resource->isExport) {
      continue;
    }
    std::string tempPagPath = JoinPaths(GetTempFolderPath(), "tmp.pag");
    PAGExportConfigParam configParam = {};
    configParam.exportAudio = false;
    configParam.hardwareEncode = false;
    configParam.exportActually = false;
    configParam.activeItemHandle = resource->itemHandle;
    configParam.outputPath = tempPagPath;
    std::shared_ptr<PAGExport> pagExport = std::make_shared<PAGExport>(configParam);
    pagExport->exportFile();
  }
  alertInfoModel->setAlertInfos(AlertInfoManager::GetInstance().warningList);
  AlertInfoManager::GetInstance().warningList.clear();
}

QHash<int, QByteArray> CompositionsModel::roleNames() const {
  static QHash<int, QByteArray> roles = {
      {static_cast<int>(ExportCompositionModelRoles::NameRole), "name"},
      {static_cast<int>(ExportCompositionModelRoles::SavePathRole), "savePath"},
      {static_cast<int>(ExportCompositionModelRoles::IsSelectedRole), "isSelected"},
      {static_cast<int>(ExportCompositionModelRoles::IsFolderRole), "isFolder"},
      {static_cast<int>(ExportCompositionModelRoles::IsUnfoldRole), "isUnfold"},
      {static_cast<int>(ExportCompositionModelRoles::LevelRole), "level"},
  };
  return roles;
}

}  // namespace exporter
