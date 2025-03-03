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
#include "CompositionModel.h"
#include <QtCore/QDebug>
#include "src/utils/AEUtils.h"

CompositionModel::CompositionModel(std::shared_ptr<AECompositionPanel> compositionPanel, QObject* parent)
    : compositionPanel(compositionPanel), QAbstractListModel(parent) {
  if (compositionPanel) {
    setCompositionData(compositionPanel->getList());
  }
}

void CompositionModel::setCompositionData(std::vector<AEComposition>* compositionData) {
  compositionVector = compositionData;
}

int CompositionModel::rowCount(const QModelIndex& parent) const {
  return compositionVector->size();
}

int CompositionModel::columnCount(const QModelIndex& parent) const {
  return 1;
}

QVariant CompositionModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid()) {
    return QVariant();
  }

  AEComposition compositionNode = compositionVector->at(index.row());
  switch (role) {
    case Qt::CheckStateRole: {
      bool isChecked = compositionNode.isBmp || !compositionNode.notAllInBmp;
      return QVariant(isChecked);
    }
    case Qt::DisplayRole:
      return QString(compositionNode.name.c_str());
    case CAN_CHECK_ROLE: {
      bool isEnable = compositionNode.notAllInBmp;
      return QVariant(isEnable);
    }
    case OFFSET_X_ROLE: {
      bool firstItem = index.row() == 0;
      int offsetX = firstItem ? 15 : 48;
      return offsetX;
    }
    default:
      return QVariant();
  }
}

bool CompositionModel::setData(const QModelIndex& index, const QVariant& value, int role) {
  if (!index.isValid()) {
    return false;
  }
  AEComposition& nodeData = compositionVector->at(index.row());
  nodeData.isBmp = value.toBool();
  Q_EMIT dataChanged(index, index);
  return true;
}

Qt::ItemFlags CompositionModel::flags(const QModelIndex& index) const {
  if (!index.isValid()) {
    return QAbstractItemModel::flags(index);
  }

  bool isEnable = index.data(CAN_CHECK_ROLE).toBool();
  if (isEnable) {
    // 折叠文件夹设置可以点击
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
  }
  return Qt::NoItemFlags;
}

int CompositionModel::getSliderMaxValue() {
  if (compositionPanel == nullptr) {
    return 1;
  }
  return compositionPanel->duration;
}

QString CompositionModel::getCompositionName() {
  if (compositionPanel == nullptr) {
    return QString("");
  }
  return QString(QString(compositionPanel->compositionName.c_str()));
}

void CompositionModel::onLayerBmpStatusChange(int row, bool isChecked) {
  // 如果勾选BMP, 弹出二次确认框
  if (isChecked) {
    auto commonAlertDialog = new CommonAlertDialog();
    int result = commonAlertDialog->exec();
    if (result != QDialog::Accepted) {
      isChecked = false;
    }
    delete commonAlertDialog;
  }

  AEComposition& composition = compositionVector->at(row);
  // 先让界面刷新BMP状态，然后再去更新真实数据
  composition.isBmp = isChecked;
  auto modelIndex = index(row, 0);
  Q_EMIT dataChanged(modelIndex, modelIndex);

  if (isChecked) {
    AEUtils::EnableBmp(composition.itemH);
  } else {
    AEUtils::DisableBmp(composition.itemH);
  }

  beginResetModel();
  std::vector<AEComposition>* compositionList = compositionPanel->getListWithRefresh();
  setCompositionData(compositionList);
  endResetModel();

  Q_EMIT bmpDataChange();
}

QHash<int, QByteArray> CompositionModel::roleNames() const {
  if (roles.empty()) {
    roles[Qt::CheckStateRole] = "itemChecked";
    roles[Qt::DisplayRole] = "name";
    roles[CAN_CHECK_ROLE] = "itemEnable";
  }
  return roles;
}
