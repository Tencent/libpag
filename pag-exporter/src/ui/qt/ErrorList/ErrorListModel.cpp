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
#include "ErrorListModel.h"
#include <QtGui/QColor>

static QColor singleRowColor(255, 255, 255, 0);
static QColor secondRowColor(255, 255, 255, 5);

ErrorListModel::ErrorListModel(QObject* parent) : QAbstractListModel(parent) {
}

ErrorListModel::~ErrorListModel() {
  alertInfoList.clear();
}

int ErrorListModel::rowCount(const QModelIndex& parent) const {
  return static_cast<int>(alertInfoList.size());
}

int ErrorListModel::columnCount(const QModelIndex& parent) const {
  return 1;
}

QVariant ErrorListModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid()) {
    return QVariant();
  }
  auto& alertInfo = alertInfoList.at(index.row());
  switch (role) {
    case Qt::BackgroundRole: {
      if (index.row() % 2 == 0) {
        return singleRowColor;
      } else {
        return secondRowColor;
      }
    }
    case ERROR_IS_ERROR_ROLE:
      return QVariant(alertInfo.isError);
    case ERROR_INFO_ROLE:
      return QString(alertInfo.info.c_str());
    case ERROR_SUGGESTION_ROLE:
      return QString(alertInfo.suggest.c_str());
    case ERROR_IS_FOLD_ROLE:
      return QVariant(alertInfo.isFold);
    case ERROR_ITEM_HEIGHT_ROLE: {
      return QVariant(alertInfo.itemHeight);
    }
    case ERROR_HAS_SUGGESTION:
      return QVariant(alertInfo.suggest.length() != 0);
    case ERROR_COMPO_NAME_ROLE:
      return QString(alertInfo.compName.c_str());
    case ERROR_LAYER_NAME_ROLE:
      return QString(alertInfo.layerName.c_str());
    case ERROR_FOLD_STATUS_CHANGE:
      return QVariant(alertInfo.isFoldStatusChange);
    case ERROR_HAS_LAYER_NAME:
      return QVariant(!alertInfo.layerName.empty());
    default:
      return QVariant();
  }
}

Qt::ItemFlags ErrorListModel::flags(const QModelIndex& index) const {
  return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

bool ErrorListModel::setData(const QModelIndex& index, const QVariant& value, int role) {
  if (!index.isValid()) {
    return false;
  }
  pagexporter::AlertInfo& alertInfo = alertInfoList.at(index.row());
  switch (role) {
    case ERROR_IS_FOLD_ROLE: {
      bool isNowFold = alertInfo.isFold;
      alertInfo.isFold = value.toBool();
      alertInfo.isFoldStatusChange = isNowFold != alertInfo.isFold;
      Q_EMIT dataChanged(index, index);
      return true;
    }
    case ERROR_ITEM_HEIGHT_ROLE: {
      alertInfo.itemHeight = value.toInt();
      Q_EMIT dataChanged(index, index);
      return true;
    }
    case ERROR_FOLD_STATUS_CHANGE: {
      alertInfo.isFoldStatusChange = value.toBool();
      return true;
    }
    default:
      return false;
  }
}

QHash<int, QByteArray> ErrorListModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[ERROR_IS_ERROR_ROLE] = "isError";
  roles[ERROR_INFO_ROLE] = "errorInfo";
  roles[ERROR_SUGGESTION_ROLE] = "suggestion";
  roles[ERROR_IS_FOLD_ROLE] = "isFold";
  roles[ERROR_ITEM_HEIGHT_ROLE] = "itemHeight";
  roles[ERROR_HAS_SUGGESTION] = "hasSuggestion";
  roles[ERROR_COMPO_NAME_ROLE] = "compositionName";
  roles[ERROR_LAYER_NAME_ROLE] = "layerName";
  roles[ERROR_HAS_LAYER_NAME] = "hasLayerName";
  return roles;
}

void ErrorListModel::setAlertInfos(std::vector<pagexporter::AlertInfo>& alertInfos) {
  // 前 3 个错误提示，默认展开
  int size = static_cast<int>(alertInfos.size());
  size = size > 3 ? 3 : size;
  for (int i = 0; i < size; i++) {
    pagexporter::AlertInfo& alertInfo = alertInfos.at(i);
    alertInfo.isFold = true;
  }

  beginResetModel();
  alertInfoList.clear();
  alertInfoList.insert(alertInfoList.begin(), alertInfos.begin(), alertInfos.end());
  endResetModel();

  Q_EMIT alertInfosChanged();
}

pagexporter::AlertInfo* ErrorListModel::getAlertInfo(const QModelIndex& index) {
  if (!index.isValid() || index.row() < 0 || index.row() >= alertInfoList.size()) {
    return nullptr;
  }
  return &alertInfoList[index.row()];
}

bool ErrorListModel::locationError(int row) {
  auto modelIndex = index(row, 0);
  auto alertInfo = getAlertInfo(modelIndex);
  if (alertInfo != nullptr) {
    alertInfo->select();
    return true;
  }
  return false;
}

bool ErrorListModel::hasErrorData() {
  return !alertInfoList.empty();
}

bool ErrorListModel::isWindows() {
#ifdef WIN32
  return true;
#else
  return false;
#endif
}

QString ErrorLayerNameData::getLayerName() {
  return layerName;
}

int ErrorLayerNameData::getAddSpaceCount() {
  return addSpaceCount;
}
