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
#include "LayerEditableModel.h"
#include <QtGui/QColor>
#include <QtCore/QDebug>
#include "src/ui/qt/ExportCommonConfig.h"

static QColor selectedColor(255, 255, 255, 25);
static QColor normalColor(255, 255, 255, 0);

LayerEditableModel::LayerEditableModel(QObject* parent) {
}

int LayerEditableModel::rowCount(const QModelIndex& parent) const {
  return placeTextData ? static_cast<int>(placeTextData->size()) : 0;
}

int LayerEditableModel::columnCount(const QModelIndex& parent) const {
  return 1;
}

QVariant LayerEditableModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || placeTextData == nullptr) {
    return QVariant();
  }

  PlaceTextLayer& itemData = placeTextData->at(index.row());
  switch (role) {
    case Qt::CheckStateRole: {
      return QVariant(itemData.editableFlag);
    }
    case TEXT_LAYER_NAME_ROLE:
      return QString(itemData.name.c_str());
    case Qt::BackgroundRole: {
      if (itemData.editableFlag) {
        return selectedColor;
      } else {
        return normalColor;
      }
    }
    default:
      return QVariant();
  }
}

QHash<int, QByteArray> LayerEditableModel::roleNames() const {
  if (roles.empty()) {
    roles[TEXT_LAYER_NAME_ROLE] = "name";
    roles[Qt::CheckStateRole] = "itemChecked";
  }
  return roles;
}

Qt::ItemFlags LayerEditableModel::flags(const QModelIndex& index) const {
  return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

bool LayerEditableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
  if (!index.isValid() || placeTextData == nullptr) {
    return false;
  }
  PlaceTextLayer& itemData = placeTextData->at(index.row());/*
  switch (role) {
    case PLACEHOLDER_SELECTED_ROLE: {
      itemData.isSelected = value.toBool();
      Q_EMIT dataChanged(index, index);
      return true;
    }
    default:
      return false;
  }//*/return false;
}

void LayerEditableModel::onLayerEditableStatusChange(int row, bool isChecked) {
  PlaceTextLayer& textLayer = placeTextData->at(row);
  textLayer.setEditable(isChecked);
  auto modelIndex = index(row, 0);
  Q_EMIT dataChanged(modelIndex, modelIndex);
}

void LayerEditableModel::setPlaceHolderData(std::vector<PlaceTextLayer>* data, bool refresh) {
  if (refresh) {
    beginResetModel();
  }
  placeTextData = data;
  if (refresh) {
    endResetModel();
  }
}

PlaceTextLayer& LayerEditableModel::getPlaceTextLayer(const QModelIndex& index) {
  if (index.row() < 0 || index.row() >= rowCount()) {
    qDebug() << "getPlaceTextLayer index invalid，index.row:" << index.row() << ", rowCount:" << rowCount();
  }
  PlaceTextLayer& placeTextLayer = placeTextData->at(index.row());
  return placeTextLayer;
}

bool LayerEditableModel::isAllSelected() {
  bool allSelected = true;
  for (auto& layer : *placeTextData) {
    if (!layer.getEditable()) {
      allSelected = false;
      break;
    }
  }
  return allSelected;
}

void LayerEditableModel::setAllChecked(bool checked) {
  for (auto& layer : *placeTextData) {
    layer.setEditable(checked);
  }
  setPlaceHolderData(placeTextData, true);
}
