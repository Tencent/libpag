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

#include "PAGFileInfoModel.h"
#include "pag/file.h"
#include "pag/pag.h"
#include "utils/PAGStringUtils.h"

namespace pag {

PAGFileInfo::PAGFileInfo(const QString& name, const QString& value, const QString& unit)
    : name(name), value(value), unit(unit) {
}

PAGFileInfoModel::PAGFileInfoModel() : QAbstractListModel(nullptr) {
}

PAGFileInfoModel::PAGFileInfoModel(QObject* parent) : QAbstractListModel(parent) {
  beginResetModel();
  fileInfos.emplace_back("Duration", "", "s");
  fileInfos.emplace_back("FrameRate", "", "FPS");
  fileInfos.emplace_back("Width");
  fileInfos.emplace_back("Height");
  fileInfos.emplace_back("Graphics");
  fileInfos.emplace_back("Videos");
  fileInfos.emplace_back("Layers");
  fileInfos.emplace_back("SDK Version");
  endResetModel();
}

auto PAGFileInfoModel::data(const QModelIndex& index, int role) const -> QVariant {
  if ((index.row() < 0) || (index.row() >= static_cast<int>(fileInfos.size()))) {
    return {};
  }

  const PAGFileInfo& fileInfo = fileInfos[index.row()];
  auto fileInfoRole = static_cast<PAGFileInfoRoles>(role);
  if (fileInfoRole == PAGFileInfoRoles::NameRole) {
    return fileInfo.name;
  }
  if (fileInfoRole == PAGFileInfoRoles::ValueRole) {
    return fileInfo.value;
  }
  if (fileInfoRole == PAGFileInfoRoles::UnitRole) {
    return fileInfo.unit;
  }

  return {};
}

auto PAGFileInfoModel::rowCount(const QModelIndex& parent) const -> int {
  Q_UNUSED(parent);
  return static_cast<int>(fileInfos.size());
}

auto PAGFileInfoModel::resetFile(const std::shared_ptr<PAGFile>& pagFile,
                                 const std::string& filePath) -> void {
  Q_UNUSED(filePath);
  beginResetModel();
  fileInfos.clear();
  fileInfos.emplace_back("Duration", Utils::toQString(pagFile->duration() / 1000000.0), "s");
  fileInfos.emplace_back("FrameRate", Utils::toQString(pagFile->frameRate()), "FPS");
  fileInfos.emplace_back("Width", Utils::toQString(pagFile->width()));
  fileInfos.emplace_back("Height", Utils::toQString(pagFile->height()));
  auto memorySize = CalculateGraphicsMemory(pagFile->getFile());
  fileInfos.emplace_back("Graphics", Utils::getMemorySizeNumString(memorySize),
                         Utils::getMemorySizeUnit(memorySize));
  fileInfos.emplace_back("Videos", Utils::toQString(pagFile->numVideos()));
  fileInfos.emplace_back("Layers", Utils::toQString(pagFile->getFile()->numLayers()));
  auto version = Utils::tagCodeToVersion(pagFile->tagLevel());
  fileInfos.emplace_back("SDK Version", version.c_str());
  endResetModel();
}

auto PAGFileInfoModel::roleNames() const -> QHash<int, QByteArray> {
  static QHash<int, QByteArray> roles = {{static_cast<int>(PAGFileInfoRoles::NameRole), "name"},
                                         {static_cast<int>(PAGFileInfoRoles::ValueRole), "value"},
                                         {static_cast<int>(PAGFileInfoRoles::UnitRole), "unit"}};
  return roles;
}

}  // namespace pag