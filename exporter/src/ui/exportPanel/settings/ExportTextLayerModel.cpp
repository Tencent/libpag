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

#include "ExportTextLayerModel.h"
#include <utility>
#include "export/Marker.h"

namespace exporter {

ExportTextLayerModel::ExportTextLayerModel(GetSessionHandler getSessionHandler, QObject* parent)
    : QAbstractListModel(parent), getSessionHandler(std::move(getSessionHandler)) {
}

void ExportTextLayerModel::setAEResource(std::shared_ptr<AEResource> newResource) {
  resource = std::move(newResource);
  refreshData(resource);
  editableItemNum = 0;
  for (const auto& item : items) {
    auto iter = resource->composition.textLayerFlagMap.find(item.layerID);
    if (iter != resource->composition.textLayerFlagMap.end() && iter->second) {
      editableItemNum++;
    }
  }
}

void ExportTextLayerModel::refreshData(std::shared_ptr<AEResource> resource) {
  if (resource->isExportAsBmp) {
    return;
  }
  auto session = getSessionHandler(this->resource->ID);
  for (const auto& layer : resource->composition.textLayers) {
    if (session == nullptr ||
        session->layerHandleMap.find(layer.layerID) == session->layerHandleMap.end()) {
      continue;
    }
    auto iter = std::find_if(items.begin(), items.end(), [layer](const AEResource::Layer& item) {
      return layer.layerID == item.layerID;
    });
    if (iter != items.end()) {
      continue;
    }
    AEResource::Layer item = {layer.layerID, layer.name.data(), layer.layerHandle};
    items.push_back(item);
    if (this->resource->composition.textLayerFlagMap.find(layer.layerID) ==
        this->resource->composition.textLayerFlagMap.end()) {
      bool isEditable = GetLayerEditable(resource->itemHandle, layer.layerID);
      this->resource->composition.textLayerFlagMap[layer.layerID] = {isEditable};
    }
  }

  for (const auto& child : resource->composition.children) {
    refreshData(child);
  }
}

void ExportTextLayerModel::setIsEditable(int row, bool isEditable) {
  if (row < 0 || static_cast<size_t>(row) >= items.size()) {
    return;
  }
  auto iter = resource->composition.textLayerFlagMap.find(items[row].layerID);
  if (iter != resource->composition.textLayerFlagMap.end() && iter->second == isEditable) {
    return;
  }
  editableItemNum += isEditable ? 1 : -1;
  resource->composition.textLayerFlagMap[items[row].layerID] = isEditable;
  SetLayerEditable(isEditable, resource->itemHandle, items[row].layerID);
  QModelIndex index = this->index(row, 0);
  Q_EMIT dataChanged(index, index, {static_cast<int>(ExportTextLayerModelRoles::IsEditableRole)});
  Q_EMIT allEditableChanged(getAllEditable());
}

void ExportTextLayerModel::setAllEditable(bool allEditable) {
  for (size_t index = 0; index < items.size(); index++) {
    setIsEditable(static_cast<int>(index), allEditable);
  }
}

bool ExportTextLayerModel::getAllEditable() const {
  return editableItemNum == items.size();
}

int ExportTextLayerModel::rowCount(const QModelIndex& parent) const {
  Q_UNUSED(parent);
  return static_cast<int>(items.size());
}

QVariant ExportTextLayerModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid()) {
    return {};
  }

  const auto& item = items[index.row()];
  switch (role) {
    case static_cast<int>(ExportTextLayerModelRoles::NumberRole): {
      return index.row();
    }
    case static_cast<int>(ExportTextLayerModelRoles::NameRole): {
      return item.name.data();
    }
    case static_cast<int>(ExportTextLayerModelRoles::IsEditableRole): {
      return resource->composition.textLayerFlagMap[item.layerID];
    }
    default:
      return {};
  }
}

void ExportTextLayerModel::onCompositionExportAsBmpChanged() {
  items.clear();
  editableItemNum = 0;
  refreshData(resource);
  for (const auto& item : items) {
    if (resource->composition.textLayerFlagMap[item.layerID]) {
      editableItemNum++;
    }
  }
  beginResetModel();
  endResetModel();
}

QHash<int, QByteArray> ExportTextLayerModel::roleNames() const {
  static QHash<int, QByteArray> roles = {
      {static_cast<int>(ExportTextLayerModelRoles::NumberRole), "number"},
      {static_cast<int>(ExportTextLayerModelRoles::NameRole), "name"},
      {static_cast<int>(ExportTextLayerModelRoles::IsEditableRole), "isEditable"},
  };
  return roles;
}

}  // namespace exporter
