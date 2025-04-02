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

FrameDisplayInfo::FrameDisplayInfo(const QString& name, const QString& color, int current, int avg, int max) :
    name(name), color(color), current(current), avg(avg), max(max) {
}

auto FrameDisplayInfo::getAVG() const -> int {
  return avg;
}

auto FrameDisplayInfo::getMAX() const -> int {
  return max;
}

auto FrameDisplayInfo::getCurrent() const -> int {
  return current;
}

auto FrameDisplayInfo::getName() const -> QString {
  return name;
}

auto FrameDisplayInfo::getColor() const -> QString {
  return color;
}


PAGFrameDisplayInfoModel::PAGFrameDisplayInfoModel(QObject* parent) : QAbstractListModel(parent) {
}

auto PAGFrameDisplayInfoModel::data(const QModelIndex& index, int role) const -> QVariant {
  if ((index.row() < 0) || (index.row() >= displayInfoList.count())) {
    return {};
  }

  const auto& item = displayInfoList.at(index.row());
  switch (role) {
    case NameRole: {
      return item.getName();
    }
    case ColorRole: {
      return item.getColor();
    }
    case CurrentRole: {
      return item.getCurrent();
    }
    case AvgRole: {
      return item.getAVG();
    }
    case MaxRole: {
      return item.getMAX();
    }
    default:
      return {};
  }
}

auto PAGFrameDisplayInfoModel::rowCount(const QModelIndex& parent) const -> int {
  Q_UNUSED(parent);
  return static_cast<int>(displayInfoList.size());
}

auto PAGFrameDisplayInfoModel::updateData(const FrameDisplayInfo& render, const FrameDisplayInfo& present, const FrameDisplayInfo& imageDecode) -> void {

  beginResetModel();
  displayInfoList.clear();
  displayInfoList.append(render);
  displayInfoList.append(imageDecode);
  displayInfoList.append(present);
  endResetModel();
}

auto PAGFrameDisplayInfoModel::roleNames() const -> QHash<int, QByteArray> {
   QHash<int, QByteArray> roles;
   roles[NameRole] = "name";
   roles[ColorRole] = "colorCode";
   roles[CurrentRole] = "current";
   roles[AvgRole] = "avg";
   roles[MaxRole] = "max";
   return roles;
}

} // namespace pag