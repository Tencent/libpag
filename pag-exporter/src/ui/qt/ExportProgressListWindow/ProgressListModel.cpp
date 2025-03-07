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
#include "ProgressListModel.h"

#include <utility>

ProgressListModel::ProgressListModel(std::vector<std::shared_ptr<ExportProgressItem>> progressItems, QObject* parent)
    : exportProgressList(std::move(progressItems)), QAbstractListModel(parent) {
}

ProgressListModel::~ProgressListModel() {
  exportProgressList.clear();
}

int ProgressListModel::rowCount(const QModelIndex& parent) const {
  return static_cast<int>(exportProgressList.size());
}

bool ifSuccess(const std::shared_ptr<ExportProgressItem>& exportProgressItem) {
  return exportProgressItem->statusCode == STATUS_SUCCESS;
}

int ProgressListModel::successCount() const {
  const int successNum = count_if(exportProgressList.begin(), exportProgressList.end(), ifSuccess);
  return successNum;
}

QVariant ProgressListModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid()) {
    return {};
  }
  auto& progressItem = exportProgressList.at(index.row());
  switch (role) {
    case PROGRESS_STATUS_CODE_ROLE:
      return {progressItem->statusCode};
    case PROGRESS_PAG_NAME_ROLE:
      return QString(progressItem->pagName.c_str());
    case PROGRESS_ERROR_INFO_ROLE:
      return QString(progressItem->errorInfo.c_str());
    case PROGRESS_ITEM_HEIGHT_ROLE:
      return {progressItem->itemHeight};
    case PROGRESS_TEXT_HEIGHT_ROLE: {
      return {progressItem->textHeight};
    }
    case PROGRESS_PROGRESS_VALUE_ROLE:
      return {progressItem->progressValue};
    default:
      return {};
  }
}

Qt::ItemFlags ProgressListModel::flags(const QModelIndex& index) const {
  return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

bool ProgressListModel::setData(const QModelIndex& index, const QVariant& value, const int role) {
  if (!index.isValid()) {
    return false;
  }
  const auto exportProgressItem = exportProgressList.at(index.row());
  switch (role) {
    case PROGRESS_STATUS_CODE_ROLE: {
      exportProgressItem->statusCode = value.toInt();
      Q_EMIT dataChanged(index, index);
      return true;
    }
    case PROGRESS_TEXT_HEIGHT_ROLE: {
      exportProgressItem->textHeight = value.toInt();
      Q_EMIT dataChanged(index, index);
      return true;
    }
    case PROGRESS_ERROR_INFO_ROLE: {
      exportProgressItem->updateErrorTextInfo(value.toString().toStdString());
      Q_EMIT dataChanged(index, index);
      return true;
    }
    case PROGRESS_ITEM_HEIGHT_ROLE: {
      exportProgressItem->itemHeight = value.toInt();
      Q_EMIT dataChanged(index, index);
      return true;
    }
    case PROGRESS_PROGRESS_VALUE_ROLE: {
      exportProgressItem->progressValue = value.toFloat();
      Q_EMIT dataChanged(index, index);
      return true;
    }
    default:
      return false;
  }
}

bool ProgressListModel::setData(const int row, const QVariant& value, const QString& roleAlias) {
  const int role = getRoleFromName(roleAlias);
  const QModelIndex& modelIndex = index(row, 0);
  return setData(modelIndex, value, role);
}

QHash<int, QByteArray> ProgressListModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[PROGRESS_STATUS_CODE_ROLE] = "statusCode";
  roles[PROGRESS_ERROR_INFO_ROLE] = "errorInfo";
  roles[PROGRESS_PAG_NAME_ROLE] = "pagName";
  roles[PROGRESS_TEXT_HEIGHT_ROLE] = "textHeight";
  roles[PROGRESS_ITEM_HEIGHT_ROLE] = "itemHeight";
  roles[PROGRESS_PROGRESS_VALUE_ROLE] = "progressValue";
  return roles;
}

int ProgressListModel::getRoleFromName(const QString& roleAlias) const {
  const auto roles = roleNames();
  if (roles.empty()) {
    return Qt::UserRole;
  }
  const auto begin = roles.constBegin();
  for (auto index = begin; index != roles.constEnd(); index++) {
    if (QString roleName(index.value()); roleName == roleAlias) {
      return index.key();
    }
  }
  return Qt::UserRole;
}

void ProgressListModel::handlePreviewIconClick(const int row) {
  const auto modelIndex = index(row, 0);
  Q_EMIT previewIconClicked(modelIndex);
}

void ProgressListModel::setProgressList(std::vector<std::shared_ptr<ExportProgressItem>> progressList) {
  beginResetModel();
  exportProgressList.clear();
  exportProgressList.insert(exportProgressList.begin(), progressList.begin(), progressList.end());
  endResetModel();
}

std::shared_ptr<ExportProgressItem> ProgressListModel::getProgressItem(const QModelIndex& index) {
  if (!index.isValid() || index.row() < 0 || index.row() >= exportProgressList.size()) {
    return nullptr;
  }
  return exportProgressList[index.row()];
}

bool ProgressListModel::isWindows() {
#ifdef WIN32
  return true;
#else
  return false;
#endif
}