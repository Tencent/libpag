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

#pragma once
#include <QAbstractListModel>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QVariantList>
#include <memory>
#include "utils/AlertInfo.h"

namespace exporter {

class AlertInfoModel : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(
      QString errorMessage READ getErrorMessage WRITE setErrorMessage NOTIFY errorMessageChanged)
  Q_PROPERTY(int count READ getCount NOTIFY alertCountChanged)

 public:
  enum class AlertRoles {
    IsErrorRole = Qt::UserRole + 1,
    InfoRole,
    SuggestionRole,
    IsFoldRole,
    CompoNameRole,
    LayerNameRole,
    HasLayerNameRole,
    HasSuggestionRole
  };
  Q_ENUM(AlertRoles)

  static AlertRoles IntToAlertRole(int role) {
    return static_cast<AlertRoles>(role);
  }

  explicit AlertInfoModel(QObject* parent = nullptr);
  ~AlertInfoModel() override;

  int rowCount(const QModelIndex& parent) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role) override;

  Q_INVOKABLE bool locateAlert(int row);
  Q_INVOKABLE QVariantList getAlertInfos() const;
  Q_INVOKABLE int getAlertCount() const;
  Q_INVOKABLE static void JumpToUrl();

  QString getErrorMessage() const;
  int getCount() const;
  void setErrorMessage(const QString& message);

  void setAlertInfos(std::vector<AlertInfo> infos);

 Q_SIGNALS:
  void errorMessageChanged();
  void alertInfoChanged();
  void alertCountChanged();

 protected:
  QVariantMap alertInfoToVariantMap(const AlertInfo& alertInfo) const;
  const AlertInfo* getAlertInfo(const QModelIndex& index) const;
  bool initializeAlertWindow(const QString& qmlPath, const QString& contextName);
  bool isIndexValid(const QModelIndex& index) const;

 private:
  std::vector<AlertInfo> alertInfos = {};
  QString errorMessage = "";
  QQuickWindow* alertWindow = nullptr;
  mutable QVariantMap alertInfoVariantMap;
  static constexpr char documentationUrl[] = "https://pag.art/docs/pag-export-verify.html";
  std::string lastOutputPath = "";
  std::string lastFilePath = "";
};

}  // namespace exporter
