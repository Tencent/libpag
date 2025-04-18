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

#include "PAGFrameDisplayInfoModel.h"

namespace pag {

FrameDisplayInfo::FrameDisplayInfo(const QString& name, const QString& color, int64_t current,
                                   int64_t avg, int64_t max)
    : name(name), color(color), current(current), avg(avg), max(max) {
}

PAGFrameDisplayInfoModel::PAGFrameDisplayInfoModel() : QAbstractListModel(nullptr) {
}

PAGFrameDisplayInfoModel::PAGFrameDisplayInfoModel(QObject* parent) : QAbstractListModel(parent) {
}

auto PAGFrameDisplayInfoModel::data(const QModelIndex& index, int role) const -> QVariant {
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

auto PAGFrameDisplayInfoModel::rowCount(const QModelIndex& parent) const -> int {
  Q_UNUSED(parent);
  return static_cast<int>(diplayInfos.size());
}

auto PAGFrameDisplayInfoModel::updateData(const FrameDisplayInfo& render,
                                          const FrameDisplayInfo& present,
                                          const FrameDisplayInfo& imageDecode) -> void {

  beginResetModel();
  diplayInfos.clear();
  diplayInfos.push_back(render);
  diplayInfos.push_back(imageDecode);
  diplayInfos.push_back(present);
  endResetModel();
}

auto PAGFrameDisplayInfoModel::roleNames() const -> QHash<int, QByteArray> {
  static const QHash<int, QByteArray> roles = {
      {static_cast<int>(PAGFrameDisplayInfoRoles::NameRole), "name"},
      {static_cast<int>(PAGFrameDisplayInfoRoles::ColorRole), "colorCode"},
      {static_cast<int>(PAGFrameDisplayInfoRoles::CurrentRole), "current"},
      {static_cast<int>(PAGFrameDisplayInfoRoles::AvgRole), "avg"},
      {static_cast<int>(PAGFrameDisplayInfoRoles::MaxRole), "max"}};
  return roles;
}

}  // namespace pag