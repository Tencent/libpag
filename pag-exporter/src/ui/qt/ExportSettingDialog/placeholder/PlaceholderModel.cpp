/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "PlaceholderModel.h"
#include <QtGui/QColor>
#include <QtCore/QDebug>
#include "src/ui/qt/ExportCommonConfig.h"

static QColor selectedColor(255, 255, 255, 25);
static QColor normalColor(255, 255, 255, 0);

PlaceholderModel::PlaceholderModel(QObject* parent) {
}

int PlaceholderModel::rowCount(const QModelIndex& parent) const {
  return placeholderData ? placeholderData->size() : 0;
}

int PlaceholderModel::columnCount(const QModelIndex& parent) const {
  return 1;
}

QVariant PlaceholderModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || placeholderData == nullptr) {
    return {};
  }

  const PlaceImageLayer& itemData = placeholderData->at(index.row());
  switch (role) {
    case PLACEHOLDER_IMG_ROLE: {
      QString imageSource("image://PlaceHolderImg/");
      imageSource.append(QString::number(index.row())).append("/").append(QString(itemData.name.c_str()));
      return imageSource;
    }
    case PLACEHOLDER_NAME_ROLE:
      return QString(itemData.name.c_str());
    case PLACEHOLDER_SCALE_MODE_ROLE:
      return QVariant::fromValue(itemData.fillMode);
    case PLACEHOLDER_ASSET_MODE_ROLE:
      return QVariant::fromValue(itemData.resourceType);
    case Qt::BackgroundRole: {
      if (itemData.isSelected) {
        return selectedColor;
      } else {
        return normalColor;
      }
    }
    case PLACEHOLDER_SELECTED_ROLE:
      return itemData.isSelected;
    case Qt::CheckStateRole: {
      return {itemData.editableFlag};
    }
    default:
      return {};
  }
}

QHash<int, QByteArray> PlaceholderModel::roleNames() const {
  if (roles.empty()) {
    roles[PLACEHOLDER_IMG_ROLE] = "imageSource";
    roles[PLACEHOLDER_NAME_ROLE] = "name";
    roles[PLACEHOLDER_SCALE_MODE_ROLE] = "scaleMode";
    roles[PLACEHOLDER_ASSET_MODE_ROLE] = "assetMode";
    roles[PLACEHOLDER_SELECTED_ROLE] = "isSelected";
    roles[Qt::CheckStateRole] = "itemChecked";
  }
  return roles;
}

Qt::ItemFlags PlaceholderModel::flags(const QModelIndex& index) const {
  return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

bool PlaceholderModel::setData(const QModelIndex& index, const QVariant& value, int role) {
  if (!index.isValid() || placeholderData == nullptr) {
    return false;
  }
  PlaceImageLayer& itemData = placeholderData->at(index.row());
  return false;
}

void PlaceholderModel::setPlaceHolderData(std::vector<PlaceImageLayer>* data, bool refresh) {
  if (refresh) {
    beginResetModel();
  }
  placeholderData = data;
  if (refresh) {
    endResetModel();
  }
}

PlaceImageLayer& PlaceholderModel::getPlaceImageLayer(const QModelIndex& index) const {
  if (index.row() < 0 || index.row() >= rowCount()) {
    qDebug() << "getPlaceImageLayer index invalid，index.row:" << index.row() << ", rowCount:" << rowCount();
  }
  PlaceImageLayer& placeImageLayer = placeholderData->at(index.row());
  return placeImageLayer;
}

PlaceImageLayer& PlaceholderModel::getImageLayer(const int index) const {
  PlaceImageLayer& placeImageLayer = placeholderData->at(index);
  return placeImageLayer;
}

void PlaceholderModel::onImageEditableStatusChange(const int row, const bool isChecked) {
  PlaceImageLayer& placeImageLayer = placeholderData->at(row);
  const auto type = isChecked ? FillResourceType::Replace : FillResourceType::NoReplace;
  placeImageLayer.setResourceType(type);
  const auto modelIndex = index(row, 0);
  Q_EMIT dataChanged(modelIndex, modelIndex);
}

void PlaceholderModel::onFillModeChanged(const int row, int modeIndex) {
  PlaceImageLayer& placeImageLayer = placeholderData->at(row);
  const auto fillMode = static_cast<ImageFillMode>(modeIndex);
  placeImageLayer.setFillMode(fillMode);
  const auto modelIndex = index(row, 0);
  Q_EMIT dataChanged(modelIndex, modelIndex);
}

void PlaceholderModel::onAssetTypeChange(const int row, int typeIndex) {
  PlaceImageLayer& placeImageLayer = placeholderData->at(row);
  const auto resourceType = static_cast<FillResourceType>(typeIndex);
  placeImageLayer.setResourceType(resourceType);
  const auto modelIndex = index(row, 0);
  Q_EMIT dataChanged(modelIndex, modelIndex);
}

bool PlaceholderModel::isAllSelected() const {
  bool allSelected = true;
  for (const auto& layer : *placeholderData) {
    if (!layer.editableFlag) {
      allSelected = false;
      break;
    }
  }
  return allSelected;
}

void PlaceholderModel::setAllChecked(const bool checked) {
  for (auto& layer : *placeholderData) {
    auto type = checked ? FillResourceType::Replace : FillResourceType::NoReplace;
    layer.setResourceType(type);
  }
  setPlaceHolderData(placeholderData, true);
}
