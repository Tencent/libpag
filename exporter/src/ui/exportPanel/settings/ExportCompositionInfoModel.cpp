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

#include "ExportCompositionInfoModel.h"
#include <future>
#include <utility>
#include "utils/AEHelper.h"
#include "utils/StringHelper.h"

namespace exporter {

ExportCompositionInfoModel::ExportCompositionInfoModel(ExportFrameImageProvider* imageProvider,
                                                       GetSessionHandler getSessionHandler,
                                                       UpdateSessionHandler updateSessionHandler,
                                                       QObject* parent)
    : QAbstractListModel(parent), imageProvider(imageProvider),
      getSessionHandler(std::move(getSessionHandler)),
      updateSessionHandler(std::move(updateSessionHandler)) {
}

void ExportCompositionInfoModel::setAEResource(std::shared_ptr<AEResource> newResource) {
  resource = std::move(newResource);
  refreshData(-1, resource);
  size = GetItemDimensions(resource->itemHandle);
  duration = GetItemDuration(resource->itemHandle);
  Q_EMIT widthChanged(getWidth());
  Q_EMIT heightChanged(getHeight());
  Q_EMIT durationChanged(getDuration());
  Q_EMIT currentTimeChanged(getCurrentTime());
  Q_EMIT currentFrameChanged(getCurrentFrame());
  Q_EMIT imageProviderNameChanged(getImageProviderName());
}

void ExportCompositionInfoModel::refreshData(int parentIndex,
                                             std::shared_ptr<AEResource> resource) {
  auto session = getSessionHandler(this->resource->ID);
  if (session == nullptr ||
      session->itemHandleMap.find(resource->ID) == session->itemHandleMap.end()) {
    return;
  }
  if (parentIndex == -1) {
    items.clear();
    Data item = {0, parentIndex, true, resource};
    this->resource->composition.exportAsBmpMap[resource->ID] = item.resource->isExportAsBmp;
    items.push_back(item);
  } else {
    Data item = {items[parentIndex].level + 1, parentIndex,
                 items[parentIndex].isEnable && !items[parentIndex].resource->isExportAsBmp,
                 resource};
    this->resource->composition.exportAsBmpMap[resource->ID] =
        this->resource->composition.exportAsBmpMap[items[parentIndex].resource->ID] ||
        item.resource->isExportAsBmp;
    items.push_back(item);
  }

  int index = static_cast<int>(items.size()) - 1;
  for (const auto& child : resource->composition.children) {
    refreshData(index, child);
  }
}

int ExportCompositionInfoModel::getWidth() const {
  return size.width();
}

int ExportCompositionInfoModel::getHeight() const {
  return size.height();
}

QString ExportCompositionInfoModel::getCurrentTime() const {
  QString time;
  std::vector<pag::Frame> parts = {};
  pag::Frame value = currentFrame;
  pag::Frame base = 1;
  while (true) {
    pag::Frame part = (value / base) % 60;
    parts.push_back(part);
    base *= 60;
    if (base > value) {
      break;
    }
  }
  for (const auto& part : parts) {
    time = time.isEmpty() ? QString("%1").arg(part, 2, 10, QChar('0'))
                          : QString("%1:%2").arg(part, 2, 10, QChar('0')).arg(time);
  }
  if (time.size() == 2) {
    time = "00:" + time;
  }
  return time;
}

QString ExportCompositionInfoModel::getDuration() const {
  return QString::number(duration);
}

QString ExportCompositionInfoModel::getCurrentFrame() const {
  return QString::number(currentFrame);
}

QString ExportCompositionInfoModel::getImageProviderName() const {
  return imageProvider->getName();
}

void ExportCompositionInfoModel::setCurrentFrame(const QString& currentFrame) {
  this->currentFrame = currentFrame.toLongLong();
  imageProvider->updateFrameImage(this->currentFrame);
  Q_EMIT currentTimeChanged(getCurrentTime());
  Q_EMIT currentFrameChanged(currentFrame);
}

void ExportCompositionInfoModel::setExportAsBmp(int row, bool exportAsBmp) {
  if (row < 0 || row >= static_cast<int>(items.size())) {
    return;
  }
  Data& item = items[row];
  item.resource->isExportAsBmp = exportAsBmp;
  updateSessionHandler(resource->ID);
  resource->composition.exportAsBmpMap[item.resource->ID] = exportAsBmp;
  if (exportAsBmp) {
    std::string newName = item.resource->name + CompositionBmpSuffix;
    SetItemName(item.resource->itemHandle, newName);
    item.resource->name = newName;
    item.resource->isExportAsBmp = true;
  } else {
    std::string newName = item.resource->name;
    auto pos = newName.find(CompositionBmpSuffix, newName.size() - CompositionBmpSuffix.size());
    if (pos != std::string::npos) {
      newName = newName.substr(0, pos);
    }
    SetItemName(item.resource->itemHandle, newName);
    item.resource->name = newName;
    item.resource->isExportAsBmp = false;
  }

  size_t index = row + 1;
  for (; index < items.size(); index++) {
    Data& currentItem = items[index];
    if (currentItem.level > item.level) {
      if (exportAsBmp) {
        currentItem.isEnable = false;
        resource->composition.exportAsBmpMap[currentItem.resource->ID] = true;
      } else {
        Data& parentItem = items[currentItem.parentIndex];
        currentItem.isEnable = parentItem.isEnable && !parentItem.resource->isExportAsBmp;
        if (currentItem.resource->isExportAsBmp) {
          resource->composition.exportAsBmpMap[currentItem.resource->ID] = true;
        } else {
          resource->composition.exportAsBmpMap[currentItem.resource->ID] =
              parentItem.resource->isExportAsBmp;
          ;
        }
      }
    } else {
      break;
    }
  }

  QModelIndex startIndex = this->index(row);
  QModelIndex endIndex = this->index(index - 1);
  Q_EMIT dataChanged(startIndex, endIndex,
                     {static_cast<int>(ExportCompositionInfoModelRoles::ExportAsBmpRole),
                      static_cast<int>(ExportCompositionInfoModelRoles::NameRole),
                      static_cast<int>(ExportCompositionInfoModelRoles::EnableRole)});
  Q_EMIT compositionExportAsBmpChanged();
}

bool ExportCompositionInfoModel::isCompositionHasEditableLayer(int row) {
  if (row < 0 || row >= static_cast<int>(items.size())) {
    return false;
  }
  Data& item = items[row];
  return isCompositionHasEditableLayer(item.resource);
}

bool ExportCompositionInfoModel::isCompositionHasEditableLayer(
    std::shared_ptr<AEResource> resource) {
  if (!resource->composition.imageLayers.empty() || !resource->composition.textLayers.empty()) {
    return true;
  }
  for (const auto& child : resource->composition.children) {
    if (isCompositionHasEditableLayer(child)) {
      return true;
    }
  }
  return false;
}

int ExportCompositionInfoModel::rowCount(const QModelIndex& parent) const {
  Q_UNUSED(parent);
  return static_cast<int>(items.size());
}

QVariant ExportCompositionInfoModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid()) {
    return {};
  }

  const Data& item = items[index.row()];
  switch (role) {
    case static_cast<int>(ExportCompositionInfoModelRoles::LevelRole): {
      return item.level;
    }
    case static_cast<int>(ExportCompositionInfoModelRoles::NameRole): {
      return QString(item.resource->name.data());
    }
    case static_cast<int>(ExportCompositionInfoModelRoles::ExportAsBmpRole): {
      return resource->composition.exportAsBmpMap[item.resource->ID];
    }
    case static_cast<int>(ExportCompositionInfoModelRoles::EnableRole): {
      return item.isEnable;
    }
    default: {
      return {};
    }
  }
}

QHash<int, QByteArray> ExportCompositionInfoModel::roleNames() const {
  static QHash<int, QByteArray> roles = {
      {static_cast<int>(ExportCompositionInfoModelRoles::LevelRole), "level"},
      {static_cast<int>(ExportCompositionInfoModelRoles::NameRole), "name"},
      {static_cast<int>(ExportCompositionInfoModelRoles::ExportAsBmpRole), "isExportAsBmp"},
      {static_cast<int>(ExportCompositionInfoModelRoles::EnableRole), "isEnable"},
  };
  return roles;
}

}  // namespace exporter
