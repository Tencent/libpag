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

#include "PanelExporterDataModel.h"
#include <QtCore/QDebug>
#include <iostream>
#include "ExportCommonConfig.h"
#include "TextEdit/TextEdit.h"
ExportConfigLayerModel::ExportConfigLayerModel(QObject* parent) : QAbstractTableModel(parent) {
}

ExportConfigLayerModel::~ExportConfigLayerModel() {
  originLayerData = nullptr;
}

int ExportConfigLayerModel::columnCount(const QModelIndex& parent) const {
  return 5;
}

int ExportConfigLayerModel::rowCount(const QModelIndex& parent) const {
  return layerDataList->size();
}

bool ExportConfigLayerModel::isFolderData(const QModelIndex& index) const {
  if (!index.isValid() || index.row() > layerDataList->size()) {
    return false;
  }
  auto layerData = layerDataList->at(index.row());
  return typeid(*layerData) == typeid(FolderData);
}

QVariant ExportConfigLayerModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() > layerDataList->size()) {
    return {};
  }

  std::shared_ptr<ConfigLayerData> layerData = layerDataList->at(index.row());
  switch (role) {
    case LAYER_NAME_ROLE:
      return QString::fromStdString(layerData->name);
    case SAVE_PATH_ROLE: {
      std::string path = layerData->aeResource ? layerData->aeResource->getStorePath() : "";
      return QString::fromStdString(path);
    }
    case IS_FOLDER_ROLE:
      return {isFolderData(index)};
    case IS_UNFOLD_ROLE: {
      if (!isFolderData(index)) {
        return {};
      }
      std::shared_ptr<FolderData> folderData = std::dynamic_pointer_cast<FolderData>(layerData);
      return {folderData->isUnfold};
    }
    case LAYER_LEVEL_ROLE: {
      int level = 0;
      std::shared_ptr<ConfigLayerData> tmpData = layerData;
      while (tmpData->parent != nullptr) {
        tmpData = tmpData->parent;
        level++;
      }
      return {level};
    }
    case Qt::CheckStateRole: {
      if (isFolderData(index)) {
        return {false};
      }
      return {layerData->isChecked()};
    }
    case Qt::BackgroundRole: {
      if (index.row() % 2 == 0) {
        return QString("#22222B");
      } else {
        return QString("#27272F");
      }
    }
    case AE_ITEM_DATA_ROLE:
      return QVariant::fromValue(layerData->aeResource);
    case AE_ITEM_HEIGHT_ROLE: {
      return {35};
    }
    default:
      return {};
  }
}

void ExportConfigLayerModel::unfoldFolder(const std::shared_ptr<FolderData>& folderData) {
  if (folderData == nullptr) {
    return;
  }
  int index = layerDataList->indexOf(folderData);
  for (const std::shared_ptr<ConfigLayerData>& layerData : *folderData->layerData) {
    if (!layerDataList->contains(layerData)) {
      layerDataList->insert(++index, layerData);

      // 如果子级也是目录，则递归展开
      if (typeid(*layerData) == typeid(FolderData)) {
        auto innerFolderData = std::dynamic_pointer_cast<FolderData>(layerData);
        unfoldFolder(innerFolderData);
      }
    }
  }
  folderData->isUnfold = true;

  Q_EMIT foldStatusChange();
}

void ExportConfigLayerModel::foldFolder(const std::shared_ptr<FolderData>& folderData) {
  if (folderData == nullptr) {
    return;
  }
  for (const std::shared_ptr<ConfigLayerData>& layerData : *folderData->layerData) {
    if (layerDataList->contains(layerData)) {
      layerDataList->removeOne(layerData);
      // 如果子级也是目录，则递归收折
      if (typeid(*layerData) == typeid(FolderData)) {
        auto innerFolderData = std::dynamic_pointer_cast<FolderData>(layerData);
        foldFolder(innerFolderData);
      }
    }
  }
  folderData->isUnfold = false;

  Q_EMIT foldStatusChange();
}

bool ExportConfigLayerModel::setData(const QModelIndex& index, const QVariant& value,
                                     const int role) {
  if (!index.isValid()) {
    return false;
  }
  switch (role) {
    case IS_UNFOLD_ROLE: {
      const bool unfold = value.toBool();
      auto folerData = std::dynamic_pointer_cast<FolderData>(layerDataList->at(index.row()));
      if (folerData->isUnfold == unfold) {
        return true;
      }
      beginResetModel();
      if (unfold) {
        unfoldFolder(folerData);
      } else {
        foldFolder(folerData);
      }
      endResetModel();
      return true;
    }
    case SAVE_PATH_ROLE: {
      std::shared_ptr<ConfigLayerData> layerData = layerDataList->at(index.row());
      if (layerData->aeResource && !value.toString().isEmpty()) {
        layerData->aeResource->setStorePath(value.toString().toStdString());
      }
      const auto& left = ExportConfigLayerModel::index(index.row(), 0);
      const auto& right = ExportConfigLayerModel::index(index.row(), columnCount() - 1);
      Q_EMIT dataChanged(left, right);
      return true;
    }
    case Qt::CheckStateRole: {
      //      clearAllChecked();
      std::shared_ptr<ConfigLayerData> configLayerData = layerDataList->at(index.row());
      configLayerData->setChecked(value.toBool());
      const auto& left = ExportConfigLayerModel::index(index.row(), 0);
      const auto& right = ExportConfigLayerModel::index(index.row(), columnCount() - 1);
      Q_EMIT dataChanged(left, right);
      Q_EMIT compositionCheckChanged();
      return true;
    }
    default:
      return false;
  }
}

bool ExportConfigLayerModel::setData(const int row, const QVariant& value,
                                     const QString& roleAlias) {
  int role = getRoleFromName(roleAlias);
  const QModelIndex& modelIndex = index(row, 0);
  return setData(modelIndex, value, role);
}

QmlCompositionData* ExportConfigLayerModel::get(const int row) const {
  if (layerDataList == nullptr || row < 0 || row >= layerDataList->size()) {
    return new QmlCompositionData();
  }
  auto* compositionData = new QmlCompositionData();
  compositionData->layerName = data(index(row, 0), LAYER_NAME_ROLE).toString();
  compositionData->savePath = data(index(row, 0), SAVE_PATH_ROLE).toString();
  compositionData->isFolder = data(index(row, 0), IS_FOLDER_ROLE).toBool();
  compositionData->isUnFold = data(index(row, 0), IS_UNFOLD_ROLE).toBool();
  compositionData->layerLevel = data(index(row, 0), LAYER_LEVEL_ROLE).toInt();
  compositionData->itemChecked = data(index(row, 0), Qt::CheckStateRole).toBool();
  compositionData->itemBackColor = data(index(row, 0), Qt::BackgroundRole).toString();
  return compositionData;
}

void ExportConfigLayerModel::handleSettingIconClick(const int row) {
  auto aeData = data(index(row, 0), AE_ITEM_DATA_ROLE).value<std::shared_ptr<AEResource>>();
  Q_EMIT settingIconClicked(aeData);
}

void ExportConfigLayerModel::handlePreviewIconClick(const int row) {
  auto aeData = data(index(row, 0), AE_ITEM_DATA_ROLE).value<std::shared_ptr<AEResource>>();
  Q_EMIT previewIconClicked(aeData);
}

void ExportConfigLayerModel::handleSavePathClick(const int row) {
  const auto modelIndex = index(row, 0);
  Q_EMIT savePathClicked(modelIndex);
}

bool ExportConfigLayerModel::isLayerSelected() {
  if (layerDataList == nullptr || layerDataList->empty()) {
    return false;
  }
  return !getSelectedLayer()->empty();
}

bool ExportConfigLayerModel::isAllSelected() const {
  bool allSelected = true;
  for (const std::shared_ptr<ConfigLayerData>& layerData : *layerDataList) {
    if (!layerData->isChecked()) {
      allSelected = false;
      break;
    }
  }
  return allSelected;
}

Qt::ItemFlags ExportConfigLayerModel::flags(const QModelIndex& index) const {
  if (!index.isValid()) {
    return QAbstractItemModel::flags(index);
  }
  if (isFolderData(index) && index.column() == COLUMN_LAYER_NAME) {
    // 折叠文件夹设置可以点击
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
  }
  return Qt::NoItemFlags;
}

void ExportConfigLayerModel::setLayerData(
    const std::shared_ptr<QList<std::shared_ptr<ConfigLayerData>>>& dataList,
    const bool forceRefresh) {
  beginResetModel();
  layerDataList = dataList;
  if (!originLayerData || forceRefresh) {
    originLayerData = dataList;
  }
  endResetModel();
}

void ExportConfigLayerModel::searchCompositionsByName(const QString& name) {
  const auto searchResultList = std::make_shared<QList<std::shared_ptr<ConfigLayerData>>>();
  const QString lowercaseName = name.toLower();
  for (const auto& configLayerData : *originLayerData) {
    QString configLayerDataName = QString::fromStdString(configLayerData->name);
    if (configLayerDataName.toLower().indexOf(lowercaseName) != -1) {
      searchResultList->push_back(configLayerData);
    }
  }
  setLayerData(searchResultList);

  qDebug() << "before search size:" << originLayerData->size()
           << ", after search size:" << layerDataList->size();
}

void ExportConfigLayerModel::resetAfterNoSearch() {
  if (!originLayerData) {
    return;
  }
  setLayerData(originLayerData);
}

void ExportConfigLayerModel::setAllChecked(const bool checked) {
  for (const std::shared_ptr<ConfigLayerData>& layerData : *layerDataList) {
    layerData->setChecked(checked);
  }
  setLayerData(layerDataList);
  Q_EMIT compositionCheckChanged();
}

std::shared_ptr<QList<std::shared_ptr<ConfigLayerData>>> ExportConfigLayerModel::getSelectedLayer()
    const {
  auto selectLayerlist = std::make_shared<QList<std::shared_ptr<ConfigLayerData>>>();
  for (const std::shared_ptr<ConfigLayerData>& layerData : *layerDataList) {
    if (layerData->isChecked()) {
      selectLayerlist->append(layerData);
    }
  }
  return selectLayerlist;
}

void ExportConfigLayerModel::onTitleCheckBoxStateChange(const int state) {
  beginResetModel();
  for (const std::shared_ptr<ConfigLayerData>& layerData : *this->layerDataList) {
    layerData->setChecked(state == Qt::Checked);
  }
  endResetModel();
}

void ExportConfigLayerModel::clearAllChecked() {
  int rowIndex = 0;
  for (const std::shared_ptr<ConfigLayerData>& layerData : *layerDataList) {
    const bool needNotifyChange = layerData->isChecked();
    layerData->setChecked(false);
    if (needNotifyChange) {
      auto modelIndex = index(rowIndex, 0);
      Q_EMIT dataChanged(modelIndex, modelIndex);
    }
    rowIndex++;
  }
}

QHash<int, QByteArray> ExportConfigLayerModel::roleNames() const {
  if (roles.empty()) {
    roles[LAYER_NAME_ROLE] = "layerName";
    roles[SAVE_PATH_ROLE] = "savePath";
    roles[IS_FOLDER_ROLE] = "isFolder";
    roles[IS_UNFOLD_ROLE] = "isUnFold";
    roles[LAYER_LEVEL_ROLE] = "layerLevel";
    roles[Qt::CheckStateRole] = "itemChecked";
    roles[Qt::BackgroundRole] = "itemBackColor";
    roles[AE_ITEM_HEIGHT_ROLE] = "itemHeight";
  }
  return roles;
}

int ExportConfigLayerModel::getRoleFromName(const QString& roleAlias) const {
  if (roles.empty()) {
    return Qt::UserRole;
  }
  auto begin = roles.constBegin();
  for (auto index = begin; index != roles.constEnd(); ++index) {
    if (QString roleName(index.value()); roleName == roleAlias) {
      return index.key();
    }
  }
  return Qt::UserRole;
}
