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

#include "PAGTreeViewModel.h"
#include <QJsonObject>
#include "editing/serialize/PAGTreeNode.h"

namespace pag {
PAGTreeViewModel::PAGTreeViewModel(QObject* parent) : QAbstractItemModel(parent) {
  fileTree = std::make_unique<PAGTree>();
}

QVariant PAGTreeViewModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid()) {
    return {};
  }

  auto* node = static_cast<PAGTreeNode*>(index.internalPointer());
  QJsonObject jsonObject;

  switch (static_cast<PAGTreeViewModelRoles>(role)) {
    case PAGTreeViewModelRoles::NameRole: {
      jsonObject = node->getData(0).value<QJsonObject>();
      return jsonObject.value("name").toVariant();
    }
    case PAGTreeViewModelRoles::ValueRole: {
      jsonObject = node->getData(0).value<QJsonObject>();
      return jsonObject.value("value").toVariant();
    }
    case PAGTreeViewModelRoles::IsEditableKeyRole: {
      jsonObject = node->getData(0).value<QJsonObject>();
      if (jsonObject.contains("IsEditableKey")) {
        return jsonObject.value("IsEditableKey").toVariant();
      }
      return {false};
    }
    case PAGTreeViewModelRoles::LayerIdKeyRole: {
      jsonObject = node->getData(0).value<QJsonObject>();
      if (jsonObject.contains("LayerIdKey")) {
        return jsonObject.value("LayerIdKey").toVariant();
      }
      return {-1};
    }
    case PAGTreeViewModelRoles::MarkerIndexKeyRole: {
      jsonObject = node->getData(0).value<QJsonObject>();
      if (jsonObject.contains("MarkerIndexKey")) {
        return jsonObject.value("MarkerIndexKey").toVariant();
      }
      return {-1};
    }
    default:
      return {};
  }
}

bool PAGTreeViewModel::setData(const QModelIndex& index, const QVariant& value,
                               [[maybe_unused]] int role) {
  if (!index.isValid()) {
    qDebug() << "index is invalid";
    return false;
  }

  auto* node = static_cast<PAGTreeNode*>(index.internalPointer());
  node->setValue(value);
  return true;
}

QVariant PAGTreeViewModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole) &&
      fileTree->getRootNode() != nullptr) {
    return fileTree->getRootNode()->getData(section);
  }

  return {};
}

bool PAGTreeViewModel::setHeaderData([[maybe_unused]] int section,
                                     [[maybe_unused]] Qt::Orientation orientation,
                                     [[maybe_unused]] const QVariant& value,
                                     [[maybe_unused]] int role) {
  return true;
}

Qt::ItemFlags PAGTreeViewModel::flags(const QModelIndex& index) const {
  auto value = Qt::ItemFlag::ItemIsEditable | QAbstractItemModel::flags(index);
  return value;
}

QModelIndex PAGTreeViewModel::index(int row, int column, const QModelIndex& parent) const {
  if (!parent.isValid()) {
    return createIndex(row, column, fileTree->getRootNode());
  }

  if (!hasIndex(row, column, parent)) {
    return {};
  }

  auto* parentNode = static_cast<PAGTreeNode*>(parent.internalPointer());
  auto* childNode = parentNode->getChild(row);
  if (childNode) {
    return createIndex(row, column, childNode);
  }

  return {};
}

QModelIndex PAGTreeViewModel::parent(const QModelIndex& index) const {
  if (!index.isValid()) {
    return {};
  }

  auto* childNode = static_cast<PAGTreeNode*>(index.internalPointer());
  auto* parentNode = childNode->getParent();
  if (parentNode == nullptr) {
    return {};
  }

  return createIndex(parentNode->getRow(), 0, parentNode);
}

int PAGTreeViewModel::rowCount(const QModelIndex& parent) const {
  if (!parent.isValid()) {
    return 1;
  }

  if (parent.column() > 0) {
    return 0;
  }

  return static_cast<PAGTreeNode*>(parent.internalPointer())->getChildrenCount();
}

int PAGTreeViewModel::columnCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return static_cast<PAGTreeNode*>(parent.internalPointer())->getColumnCount();
  }

  return 1;
}

QHash<int, QByteArray> PAGTreeViewModel::roleNames() const {
  static QHash<int, QByteArray> roles = {
      {static_cast<int>(PAGTreeViewModelRoles::NameRole), "name"},
      {static_cast<int>(PAGTreeViewModelRoles::ValueRole), "value"},
      {static_cast<int>(PAGTreeViewModelRoles::IsEditableKeyRole), "isEditableKey"},
      {static_cast<int>(PAGTreeViewModelRoles::LayerIdKeyRole), "layerIdKey"},
      {static_cast<int>(PAGTreeViewModelRoles::MarkerIndexKeyRole), "markerIndexKey"},
  };
  return roles;
}

void PAGTreeViewModel::setFile(std::shared_ptr<File> file) {
  fileTree->setFile(std::move(file));
  fileTree->buildTree();
  beginResetModel();
  endResetModel();
}

}  //  namespace pag
