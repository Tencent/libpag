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

#include "ProgressListModel.h"
#include "utils/AEHelper.h"
#include "utils/FileHelper.h"

namespace exporter {

ProgressListModel::ProgressListModel(QObject* parent) : QAbstractListModel(parent) {
}

void ProgressListModel::addSession(std::shared_ptr<PAGExportSession> session) {
  auto iter = std::find(sessionList.begin(), sessionList.end(), session);
  if (iter != sessionList.end()) {
    return;
  }
  int index = static_cast<int>(sessionList.size());
  connect(&session->progressModel, &ProgressModel::exportStatusChanged, this, [this, index]() {
    QModelIndex modelIndex = createIndex(index, 0);
    Q_EMIT dataChanged(modelIndex, modelIndex,
                       {static_cast<int>(ProgressListModelRoles::StatusRole)});
    Q_EMIT exportNumChanged(getExportNum());
  });
  connect(&session->progressModel, &ProgressModel::currentProgressChanged, this, [this, index]() {
    QModelIndex modelIndex = createIndex(index, 0);
    Q_EMIT dataChanged(modelIndex, modelIndex,
                       {static_cast<int>(ProgressListModelRoles::CurrentProgressRole)});
  });
  connect(&session->progressModel, &ProgressModel::totalProgressChanged, this, [this, index]() {
    QModelIndex modelIndex = createIndex(index, 0);
    Q_EMIT dataChanged(modelIndex, modelIndex,
                       {static_cast<int>(ProgressListModelRoles::TotalProgressRole)});
  });
  sessionList.push_back(std::move(session));
  Q_EMIT totalExportNumChanged(index + 1);
  beginInsertRows(QModelIndex(), index, index);
  endInsertRows();
}

void ProgressListModel::clearSessions() {
  sessionList.clear();
  beginResetModel();
  endResetModel();
  Q_EMIT exportNumChanged(0);
  Q_EMIT totalExportNumChanged(0);
}

void ProgressListModel::viewPAGFile(int row) const {
  if (row >= static_cast<int>(sessionList.size())) {
    return;
  }
  const auto& session = sessionList[row];
  OpenPAGFile(session->outputPath);
}

int ProgressListModel::getExportNum() const {
  int exportNum = 0;
  for (const auto& session : sessionList) {
    if (session->progressModel.getExportStatus() ==
        static_cast<int>(ProgressModel::ExportStatus::Success)) {
      exportNum++;
    }
  }
  return exportNum;
}

int ProgressListModel::getTotalExportNum() const {
  return static_cast<int>(sessionList.size());
}

int ProgressListModel::rowCount(const QModelIndex&) const {
  return static_cast<int>(sessionList.size());
}

QVariant ProgressListModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid()) {
    return {};
  }
  if (index.row() >= static_cast<int>(sessionList.size())) {
    return {};
  }

  const auto& session = sessionList[index.row()];
  switch (role) {
    case static_cast<int>(ProgressListModelRoles::NameRole): {
      return GetFileName(session->outputPath).c_str();
    }
    case static_cast<int>(ProgressListModelRoles::StatusRole): {
      return session->progressModel.getExportStatus();
    }
    case static_cast<int>(ProgressListModelRoles::CurrentProgressRole): {
      return session->progressModel.getCurrentProgress();
    }
    case static_cast<int>(ProgressListModelRoles::TotalProgressRole): {
      auto totalFrame = session->progressModel.getTotalProgress();
      return totalFrame == 0.0 ? 1.0 : totalFrame;
    }
    default: {
      return {};
    }
  }
}

QHash<int, QByteArray> ProgressListModel::roleNames() const {
  static QHash<int, QByteArray> roles = {
      {static_cast<int>(ProgressListModelRoles::NameRole), "name"},
      {static_cast<int>(ProgressListModelRoles::StatusRole), "exportStatus"},
      {static_cast<int>(ProgressListModelRoles::CurrentProgressRole), "currentProgress"},
      {static_cast<int>(ProgressListModelRoles::TotalProgressRole), "totalProgress"},
  };
  return roles;
}
}  // namespace exporter
