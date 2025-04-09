/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
  FrameDisplayInfo(const QString& name, const QString& color, int current, int avg, int max);

  auto getAVG() const -> int;
  auto getMAX() const -> int;
  auto getCurrent() const -> int;
  auto getName() const -> QString;
  auto getColor() const -> QString;

 public:
  QString name;
  QString color;
  int current;
  int avg;
  int max;
};

class PAGFrameDisplayInfoModel : public QAbstractListModel {
  Q_OBJECT
 public:
  enum PAGFrameDisplayInfoRoles {
    NameRole = Qt::UserRole + 1,
    ColorRole,
    CurrentRole,
    AvgRole,
    MaxRole
  };

  explicit PAGFrameDisplayInfoModel(QObject* parent = nullptr);

  auto data(const QModelIndex& index, int role) const -> QVariant override;
  auto rowCount(const QModelIndex& parent) const -> int override;
  auto updateData(const FrameDisplayInfo& render, const FrameDisplayInfo& present,
                  const FrameDisplayInfo& imageDecode) -> void;

 protected:
  auto roleNames() const -> QHash<int, QByteArray> override;

 private:
  QList<FrameDisplayInfo> displayInfoList;
};

}  // namespace pag