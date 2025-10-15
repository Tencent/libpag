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

#include "AlertInfoModel.h"
#include <QApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QQmlContext>
#include <QThread>
#include "WindowManager.h"
#include "utils/AEHelper.h"
#include "utils/StringHelper.h"

namespace exporter {

AlertInfoModel::AlertInfoModel(QObject* parent) : QAbstractListModel(parent) {
}

AlertInfoModel::~AlertInfoModel() {
  alertInfos.clear();
}

int AlertInfoModel::rowCount(const QModelIndex& parent) const {
  Q_UNUSED(parent)
  return static_cast<int>(alertInfos.size());
}

QVariant AlertInfoModel::data(const QModelIndex& index, const int role) const {
  if (!isIndexValid(index)) {
    return {};
  }

  const AlertInfo& alertInfo = alertInfos[index.row()];

  switch (IntToAlertRole(role)) {
    case AlertRoles::IsErrorRole:
      return alertInfo.isError;
    case AlertRoles::InfoRole:
      return QString::fromStdString(alertInfo.info);
    case AlertRoles::SuggestionRole:
      return QString::fromStdString(alertInfo.suggest);
    case AlertRoles::IsFoldRole:
      return alertInfo.isFold;
    case AlertRoles::CompoNameRole:
      return QString::fromStdString(alertInfo.compName);
    case AlertRoles::LayerNameRole:
      return QString::fromStdString(alertInfo.layerName);
    case AlertRoles::HasLayerNameRole:
      return !alertInfo.layerName.empty();
    case AlertRoles::HasSuggestionRole:
      return !alertInfo.suggest.empty();
    default:
      return {};
  }
}

QHash<int, QByteArray> AlertInfoModel::roleNames() const {
  static const QHash<int, QByteArray> roles = {
      {static_cast<int>(AlertRoles::IsErrorRole), "isError"},
      {static_cast<int>(AlertRoles::InfoRole), "errorInfo"},
      {static_cast<int>(AlertRoles::SuggestionRole), "suggestion"},
      {static_cast<int>(AlertRoles::IsFoldRole), "isFold"},
      {static_cast<int>(AlertRoles::CompoNameRole), "compositionName"},
      {static_cast<int>(AlertRoles::LayerNameRole), "layerName"},
      {static_cast<int>(AlertRoles::HasLayerNameRole), "hasLayerName"},
      {static_cast<int>(AlertRoles::HasSuggestionRole), "hasSuggestion"}};
  return roles;
}

bool AlertInfoModel::setData(const QModelIndex& index, const QVariant& value, const int role) {
  if (!isIndexValid(index)) {
    return false;
  }

  AlertInfo& alertInfo = alertInfos[index.row()];

  if (IntToAlertRole(role) == AlertRoles::IsFoldRole) {
    alertInfo.isFold = value.toBool();
    Q_EMIT dataChanged(index, index);
    return true;
  }
  return false;
}

bool AlertInfoModel::locateAlert(const int row) {
  if (row < 0 || row >= static_cast<int>(alertInfos.size())) {
    return false;
  }

  AlertInfo& alertInfo = alertInfos[row];
  alertInfo.select();
  return true;
}

QVariantList AlertInfoModel::getAlertInfos() const {
  QVariantList result;
  for (const AlertInfo& alertInfo : alertInfos) {
    result.append(alertInfoToVariantMap(alertInfo));
  }
  return result;
}

int AlertInfoModel::getAlertCount() const {
  return static_cast<int>(alertInfos.size());
}

void AlertInfoModel::JumpToUrl() {
  QDesktopServices::openUrl(QUrl(QLatin1String(documentationUrl)));
}

void AlertInfoModel::setAlertInfos(std::vector<AlertInfo> infos) {
  beginResetModel();
  alertInfos = std::move(infos);
  endResetModel();

  Q_EMIT alertInfoChanged();
  Q_EMIT alertCountChanged();
}

QVariantMap AlertInfoModel::alertInfoToVariantMap(const AlertInfo& alertInfo) const {
  alertInfoVariantMap.clear();

  alertInfoVariantMap["isError"] = alertInfo.isError;
  alertInfoVariantMap["errorInfo"] = QString::fromStdString(alertInfo.info);
  alertInfoVariantMap["suggestion"] = QString::fromStdString(alertInfo.suggest);
  alertInfoVariantMap["isFold"] = alertInfo.isFold;
  alertInfoVariantMap["compositionName"] = QString::fromStdString(alertInfo.compName);
  alertInfoVariantMap["layerName"] = QString::fromStdString(alertInfo.layerName);
  alertInfoVariantMap["hasLayerName"] = !alertInfo.layerName.empty();
  alertInfoVariantMap["hasSuggestion"] = !alertInfo.suggest.empty();

  return alertInfoVariantMap;
}

const AlertInfo* AlertInfoModel::getAlertInfo(const QModelIndex& index) const {
  if (!isIndexValid(index)) {
    return nullptr;
  }
  return &alertInfos[index.row()];
}

bool AlertInfoModel::isIndexValid(const QModelIndex& index) const {
  return index.isValid() && index.row() >= 0 && index.row() < static_cast<int>(alertInfos.size());
}

QString AlertInfoModel::getErrorMessage() const {
  return errorMessage;
}

int AlertInfoModel::getCount() const {
  return static_cast<int>(alertInfos.size());
}

void AlertInfoModel::setErrorMessage(const QString& message) {
  if (errorMessage != message) {
    errorMessage = message;
    Q_EMIT errorMessageChanged();
  }
}

}  // namespace exporter
