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

#include "ExportImageLayerModel.h"
#include "export/Marker.h"
#include "utils/AEHelper.h"

namespace exporter {

static pag::PAGScaleMode QStringToScaleMode(const QString& str) {
  if (QString::compare(str, "none", Qt::CaseInsensitive) == 0) {
    return pag::PAGScaleMode::None;
  } else if (QString::compare(str, "stretch", Qt::CaseInsensitive) == 0) {
    return pag::PAGScaleMode::Stretch;
  } else if (QString::compare(str, "zoom", Qt::CaseInsensitive) == 0) {
    return pag::PAGScaleMode::Zoom;
  } else if (QString::compare(str, "letterbox", Qt::CaseInsensitive) == 0) {
    return pag::PAGScaleMode::LetterBox;
  }
  return pag::PAGScaleMode::LetterBox;
}

static QString ScaleModeToQString(pag::PAGScaleMode mode) {
  switch (mode) {
    case pag::PAGScaleMode::None:
      return "None";
    case pag::PAGScaleMode::Stretch:
      return "Stretch";
    case pag::PAGScaleMode::LetterBox:
      return "LetterBox";
    case pag::PAGScaleMode::Zoom:
      return "Zoom";
    default:
      return "LetterBox";
  }
}

ExportImageLayerModel::ExportImageLayerModel(GetSessionHandler getSessionHandler, QObject* parent)
    : QAbstractListModel(parent), getSessionHandler(getSessionHandler) {
}

void ExportImageLayerModel::setAEResource(std::shared_ptr<AEResource> newResource) {
  resource = std::move(newResource);
  refreshData(resource);
  editableItemNum = 0;
  for (const auto& item : items) {
    if (resource->composition.imagesLayerFlagMap[item.layerID].isEditable) {
      editableItemNum++;
    }
  }
}

void ExportImageLayerModel::refreshData(std::shared_ptr<AEResource> resource) {
  if (resource->isExportAsBmp) {
    return;
  }
  auto session = getSessionHandler(this->resource->ID);
  for (const auto& layer : resource->composition.imageLayers) {
    if (session == nullptr ||
        session->layerHandleMap.find(layer.layerID) == session->layerHandleMap.end()) {
      continue;
    }
    auto iter = std::find_if(items.begin(), items.end(), [layer](const AEResource::Layer& item) {
      return GetLayerItemH(layer.layerHandle) == GetLayerItemH(item.layerHandle);
    });
    if (iter != items.end()) {
      continue;
    }
    AEResource::Layer item = layer;
    items.push_back(item);
    if (this->resource->composition.imagesLayerFlagMap.find(layer.layerID) ==
        this->resource->composition.imagesLayerFlagMap.end()) {
      pag::ID imageID = GetItemID(GetLayerItemH(layer.layerHandle));
      auto mode = GetImageFillMode(resource->itemHandle, imageID);
      this->resource->composition.imagesLayerFlagMap[layer.layerID] = {true, mode};
    }
  }

  for (const auto& child : resource->composition.children) {
    refreshData(child);
  }
}

void ExportImageLayerModel::setIsEditable(int row, bool isEditable) {
  if (row < 0 || static_cast<size_t>(row) >= items.size()) {
    return;
  }
  if (resource->composition.imagesLayerFlagMap[items[row].layerID].isEditable == isEditable) {
    return;
  }
  editableItemNum += isEditable ? 1 : -1;
  resource->composition.imagesLayerFlagMap[items[row].layerID].isEditable = isEditable;
  pag::ID imageID = GetItemID(GetLayerItemH(items[row].layerHandle));
  SetLayerEditable(isEditable, resource->itemHandle, imageID);
  QModelIndex index = this->index(row, 0);
  Q_EMIT dataChanged(index, index, {static_cast<int>(ExportImageLayerModelRoles::IsEditableRole)});
  Q_EMIT allEditableChanged(getAllEditable());
}

void ExportImageLayerModel::setAllEditable(bool allEditable) {
  for (size_t index = 0; index < items.size(); index++) {
    setIsEditable(static_cast<int>(index), allEditable);
  }
}

void ExportImageLayerModel::setScaleMode(int row, const QString& scaleMode) {
  if (row < 0 || static_cast<size_t>(row) >= items.size()) {
    return;
  }
  pag::PAGScaleMode mode = QStringToScaleMode(scaleMode);
  if (resource->composition.imagesLayerFlagMap[items[row].layerID].scaleMode == mode) {
    return;
  }
  resource->composition.imagesLayerFlagMap[items[row].layerID].scaleMode = mode;
  pag::ID imageID = GetItemID(GetLayerItemH(items[row].layerHandle));
  SetImageFillMode(mode, resource->itemHandle, imageID);
  QModelIndex index = this->index(row, 0);
  Q_EMIT dataChanged(index, index, {static_cast<int>(ExportImageLayerModelRoles::FillModeRole)});
}

bool ExportImageLayerModel::getAllEditable() const {
  return editableItemNum == items.size();
}

QStringList ExportImageLayerModel::getScaleModes() const {
  static QStringList scaleModes = {"None", "Stretch", "LetterBox", "Zoom"};
  return scaleModes;
}

int ExportImageLayerModel::rowCount(const QModelIndex& parent) const {
  Q_UNUSED(parent);
  return static_cast<int>(items.size());
}

QVariant ExportImageLayerModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid()) {
    return {};
  }

  const auto& item = items[index.row()];
  switch (role) {
    case static_cast<int>(ExportImageLayerModelRoles::NumberRole): {
      return index.row();
    }
    case static_cast<int>(ExportImageLayerModelRoles::NameRole): {
      return item.name.data();
    }
    case static_cast<int>(ExportImageLayerModelRoles::IsEditableRole): {
      return resource->composition.imagesLayerFlagMap[item.layerID].isEditable;
    }
    case static_cast<int>(ExportImageLayerModelRoles::FillModeRole): {
      return ScaleModeToQString(resource->composition.imagesLayerFlagMap[item.layerID].scaleMode);
    }
    default:
      return {};
  }
}

void ExportImageLayerModel::onCompositionExportAsBmpChanged() {
  items.clear();
  editableItemNum = 0;
  refreshData(resource);
  for (const auto& item : items) {
    if (resource->composition.imagesLayerFlagMap[item.layerID].isEditable) {
      editableItemNum++;
    }
  }
  beginResetModel();
  endResetModel();
}

QHash<int, QByteArray> ExportImageLayerModel::roleNames() const {
  static QHash<int, QByteArray> roles = {
      {static_cast<int>(ExportImageLayerModelRoles::NumberRole), "number"},
      {static_cast<int>(ExportImageLayerModelRoles::NameRole), "name"},
      {static_cast<int>(ExportImageLayerModelRoles::IsEditableRole), "isEditable"},
      {static_cast<int>(ExportImageLayerModelRoles::FillModeRole), "fillMode"},
  };
  return roles;
}

}  // namespace exporter
