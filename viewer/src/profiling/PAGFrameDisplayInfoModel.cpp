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

#include "PAGFrameDisplayInfoModel.h"

namespace pag {

FrameDisplayInfo::FrameDisplayInfo(const QString& name, const QString& color, int64_t current,
                                   int64_t avg, int64_t max)
    : name(name), color(color), current(current), avg(avg), max(max) {
}

PAGFrameDisplayInfoModel::PAGFrameDisplayInfoModel() : QAbstractListModel(nullptr) {
  diplayInfos.resize(3);
}

PAGFrameDisplayInfoModel::PAGFrameDisplayInfoModel(QObject* parent) : QAbstractListModel(parent) {
  diplayInfos.resize(3);
}

QVariant PAGFrameDisplayInfoModel::data(const QModelIndex& index, int role) const {
  if ((index.row() < 0) || (index.row() >= static_cast<int>(diplayInfos.size()))) {
    return {};
  }

  const auto& item = diplayInfos[index.row()];
  switch (static_cast<PAGFrameDisplayInfoRoles>(role)) {
    case PAGFrameDisplayInfoRoles::NameRole: {
      return item.name;
    }
    case PAGFrameDisplayInfoRoles::ColorRole: {
      return item.color;
    }
    case PAGFrameDisplayInfoRoles::CurrentRole: {
      return QString::number(item.current);
    }
    case PAGFrameDisplayInfoRoles::AvgRole: {
      return QString::number(item.avg);
    }
    case PAGFrameDisplayInfoRoles::MaxRole: {
      return QString::number(item.max);
    }
    default:
      return {};
  }
}

int PAGFrameDisplayInfoModel::rowCount(const QModelIndex& parent) const {
  Q_UNUSED(parent);
  return static_cast<int>(diplayInfos.size());
}

void PAGFrameDisplayInfoModel::updateData(const FrameDisplayInfo& render,
                                          const FrameDisplayInfo& present,
                                          const FrameDisplayInfo& imageDecode) {

  diplayInfos[0] = render;
  diplayInfos[1] = imageDecode;
  diplayInfos[2] = present;
  dataChanged(createIndex(0, 0), createIndex(static_cast<int>(diplayInfos.size()) - 1, 0));
}

QHash<int, QByteArray> PAGFrameDisplayInfoModel::roleNames() const {
  static const QHash<int, QByteArray> roles = {
      {static_cast<int>(PAGFrameDisplayInfoRoles::NameRole), "name"},
      {static_cast<int>(PAGFrameDisplayInfoRoles::ColorRole), "colorCode"},
      {static_cast<int>(PAGFrameDisplayInfoRoles::CurrentRole), "current"},
      {static_cast<int>(PAGFrameDisplayInfoRoles::AvgRole), "avg"},
      {static_cast<int>(PAGFrameDisplayInfoRoles::MaxRole), "max"}};
  return roles;
}

}  // namespace pag
