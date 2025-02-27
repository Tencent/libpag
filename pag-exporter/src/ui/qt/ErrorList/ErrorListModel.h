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
#ifndef ERRORLIST_H
#define ERRORLIST_H
#include <QtCore/QAbstractListModel>
#include "AlertInfo.h"
#include "ExportCommonConfig.h"

class ErrorListModel : public QAbstractListModel {
  Q_OBJECT
 public:
  ErrorListModel(QObject* parent = nullptr);
  ~ErrorListModel();

  Q_INVOKABLE int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  Q_INVOKABLE int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  Q_INVOKABLE QVariant data(const QModelIndex& index, int role) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  Q_INVOKABLE bool setData(const QModelIndex& index, const QVariant& value,
                           int role = Qt::EditRole) override;
  virtual QHash<int, QByteArray> roleNames() const;
  Q_INVOKABLE bool hasErrorData();
  Q_INVOKABLE bool isWindows();
  Q_INVOKABLE bool locationError(int row);

  void setAlertInfos(std::vector<pagexporter::AlertInfo>& alertInfos);
  pagexporter::AlertInfo* getAlertInfo(const QModelIndex& index);

  Q_SIGNAL void alertInfosChanged();

 private:
  std::vector<pagexporter::AlertInfo> alertInfoList;
};

class ErrorLayerNameData : public QObject {
  Q_OBJECT
 public:
  Q_INVOKABLE explicit ErrorLayerNameData(QObject* parent = nullptr) : QObject(parent){};
  Q_INVOKABLE QString getLayerName();
  Q_INVOKABLE int getAddSpaceCount();

  QString layerName;
  int addSpaceCount;
};

#endif  //ERRORLIST_H
