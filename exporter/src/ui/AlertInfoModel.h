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

 public:
  explicit AlertInfoModel(QObject* parent = nullptr);
  ~AlertInfoModel() override;

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

  Q_INVOKABLE bool locateAlert(int row) const;
  Q_INVOKABLE QVariantList getAlertInfos() const;
  Q_INVOKABLE int getAlertCount() const;
  Q_INVOKABLE void jumpToUrl();

  Q_PROPERTY(
      QString errorMessage READ getErrorMessage WRITE setErrorMessage NOTIFY errorMessageChanged)

  enum AlertRoles {
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

  void setAlertInfos(std::vector<AlertInfo>& infos);

  Q_SIGNAL void alertInfoChanged();

  QString getErrorMessage() const;
  void setErrorMessage(const QString& message);

  Q_SIGNAL void errorMessageChanged();

  bool WarningsAlert(std::vector<AlertInfo>& infos);
  bool ErrorsAlert(std::vector<AlertInfo>& info);
  std::string browseForSave(bool useScript);

 private:
  std::vector<AlertInfo> alertInfos;
  QString errorMessage;
  std::unique_ptr<QApplication> app = nullptr;
  std::unique_ptr<QQmlApplicationEngine> alertEngine = nullptr;
  QQuickWindow* alertWindow = nullptr;

  QVariantMap alertInfoToVariantMap(const AlertInfo& alertInfo) const;
  const AlertInfo* getAlertInfo(const QModelIndex& index) const;
};

}  // namespace exporter
