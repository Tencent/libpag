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
#ifndef PROGRESSLISTMODEL_H
#define PROGRESSLISTMODEL_H
#include <QtCore/QAbstractListModel>
#include <algorithm>
#include "src/ui/qt/ExportCommonConfig.h"
#include "src/utils/ExportProgressItem.h"
#include "src/utils/AEResource.h"

class ProgressListModel : public QAbstractListModel {
  Q_OBJECT
 public:
  explicit ProgressListModel(std::vector<std::shared_ptr<ExportProgressItem>> progressItems, QObject* parent = nullptr);
  ~ProgressListModel() override;

  Q_INVOKABLE int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  Q_INVOKABLE int successCount() const;
  Q_INVOKABLE QVariant data(const QModelIndex& index, int role) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  Q_INVOKABLE bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
  Q_INVOKABLE bool setData(const int row, const QVariant& value, const QString& roleAlias);
  Q_INVOKABLE void handlePreviewIconClick(int row);
  QHash<int, QByteArray> roleNames() const override;
  int getRoleFromName(const QString& roleAlias) const;
  Q_INVOKABLE bool isWindows();

  void setProgressList(std::vector<std::shared_ptr<ExportProgressItem>> progressList);
  std::shared_ptr<ExportProgressItem> getProgressItem(const QModelIndex& index);

  Q_SIGNAL void previewIconClicked(const QModelIndex& index);


private:
  std::vector<std::shared_ptr<ExportProgressItem>> exportProgressList;
};
#endif //PROGRESSLISTMODEL_H
