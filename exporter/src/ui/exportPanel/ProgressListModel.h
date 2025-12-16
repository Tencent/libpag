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

#pragma once

#include <QAbstractListModel>
#include "utils/PAGExportSession.h"

namespace exporter {

class ProgressListModel : public QAbstractListModel {
  Q_OBJECT
 public:
  enum class ProgressListModelRoles {
    NameRole = Qt::UserRole + 1,
    StatusRole,
    CurrentProgressRole,
    TotalProgressRole
  };

  Q_PROPERTY(int exportNum READ getExportNum NOTIFY exportNumChanged)
  Q_PROPERTY(int totalExportNum READ getTotalExportNum NOTIFY totalExportNumChanged)

  explicit ProgressListModel(QObject* parent = nullptr);
  void addSession(std::shared_ptr<PAGExportSession> session);
  void clearSessions();

  Q_INVOKABLE void viewPAGFile(int row) const;
  Q_INVOKABLE int getExportNum() const;
  Q_INVOKABLE int getTotalExportNum() const;

  int rowCount(const QModelIndex& parent) const override;
  QVariant data(const QModelIndex& index, int role) const override;

  Q_SIGNAL void exportNumChanged(int exportNum);
  Q_SIGNAL void totalExportNumChanged(int totalExportNum);

 protected:
  QHash<int, QByteArray> roleNames() const override;

 private:
  std::vector<std::shared_ptr<PAGExportSession>> sessionList = {};
};

}  // namespace exporter
