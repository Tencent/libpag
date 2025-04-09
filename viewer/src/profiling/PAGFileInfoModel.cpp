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

PAGFileInfo::PAGFileInfo(const QString& name, const QString& value, const QString& ext)
    : name(name), value(value), ext(ext) {
}

auto PAGFileInfo::getExt() const -> QString {
  return ext;
}

auto PAGFileInfo::getName() const -> QString {
  return name;
}

auto PAGFileInfo::getValue() const -> QString {
  return value;
}

PAGFileInfoModel::PAGFileInfoModel(QObject* parent) : QAbstractListModel(parent) {
  beginResetModel();
  fileInfoList.append(PAGFileInfo("Duration", "", "s"));
  fileInfoList.append(PAGFileInfo("FrameRate", "", "FPS"));
  fileInfoList.append(PAGFileInfo("Width"));
  fileInfoList.append(PAGFileInfo("Height"));
  fileInfoList.append(PAGFileInfo("Graphics"));
  fileInfoList.append(PAGFileInfo("Videos"));
  fileInfoList.append(PAGFileInfo("Layers"));
  fileInfoList.append(PAGFileInfo("SDK Version"));
  endResetModel();
}

auto PAGFileInfoModel::data(const QModelIndex& index, int role) const -> QVariant {
  if ((index.row() < 0) || (index.row() >= fileInfoList.count())) {
    return {};
  }

  const PAGFileInfo& fileInfo = fileInfoList[index.row()];
  if (role == NameRole) {
    return fileInfo.getName();
  } else if (role == ValueRole) {
    return fileInfo.getValue();
  } else if (role == ExtRole) {
    return fileInfo.getExt();
  }

  return {};
}

auto PAGFileInfoModel::rowCount(const QModelIndex& parent) const -> int {
  Q_UNUSED(parent);
  return static_cast<int>(fileInfoList.count());
}

auto PAGFileInfoModel::resetFile(const std::shared_ptr<PAGFile>& pagFile, std::string filePath)
    -> void {
  Q_UNUSED(filePath);
  beginResetModel();
  updateFileInfo(PAGFileInfo("Duration", Utils::toQString(pagFile->duration() / 1000000.0)));
  updateFileInfo(PAGFileInfo("FrameRate", Utils::toQString(pagFile->frameRate())));
  updateFileInfo(PAGFileInfo("Width", Utils::toQString(pagFile->width())));
  updateFileInfo(PAGFileInfo("Height", Utils::toQString(pagFile->height())));
  auto memorySize = CalculateGraphicsMemory(pagFile->getFile());
  updateFileInfo(PAGFileInfo("Graphics", Utils::getMemorySizeNumString(memorySize),
                             Utils::getMemorySizeUnit(memorySize)));
  updateFileInfo(PAGFileInfo("Videos", Utils::toQString(pagFile->numVideos())));
  updateFileInfo(PAGFileInfo("Layers", Utils::toQString(pagFile->getFile()->numLayers())));
  auto version = Utils::tagCodeToVersion(pagFile->tagLevel());
  updateFileInfo(PAGFileInfo("SDK Version", version.c_str()));
  endResetModel();
}

auto PAGFileInfoModel::roleNames() const -> QHash<int, QByteArray> {
  QHash<int, QByteArray> roles;
  roles[NameRole] = "name";
  roles[ValueRole] = "value";
  roles[ExtRole] = "ext";
  return roles;
}

auto PAGFileInfoModel::updateFileInfo(const PAGFileInfo& fileInfo) -> void {
  for (auto& info : fileInfoList) {
    if (info.getName().compare(fileInfo.getName()) == 0) {
      info.value = fileInfo.getValue();
      if (!fileInfo.getExt().isEmpty()) {
        info.ext = fileInfo.getExt();
      }
      break;
    }
  }
}

}  // namespace pag