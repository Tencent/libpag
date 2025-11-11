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
#include <QString>

namespace pag {

class FrameDisplayInfo {
 public:
  FrameDisplayInfo(const QString& name = "", const QString& color = "", int64_t current = 0,
                   int64_t avg = 0, int64_t max = 0);

  QString name = "";
  QString color = "";
  int64_t current = 0;
  int64_t avg = 0;
  int64_t max = 0;
};

class PAGFrameDisplayInfoModel : public QAbstractListModel {
  Q_OBJECT
 public:
  enum class PAGFrameDisplayInfoRoles {
    NameRole = Qt::UserRole + 1,
    ColorRole,
    CurrentRole,
    AvgRole,
    MaxRole
  };

  PAGFrameDisplayInfoModel();
  explicit PAGFrameDisplayInfoModel(QObject* parent);

  QVariant data(const QModelIndex& index, int role) const override;
  int rowCount(const QModelIndex& parent) const override;
  void updateData(const FrameDisplayInfo& render, const FrameDisplayInfo& present,
                  const FrameDisplayInfo& imageDecode);

 protected:
  QHash<int, QByteArray> roleNames() const override;

 private:
  std::vector<FrameDisplayInfo> diplayInfos = {};
};

}  // namespace pag