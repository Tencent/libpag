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
#ifndef PLACEHOLDERMODEL_H
#define PLACEHOLDERMODEL_H
#include <QtCore/QAbstractListModel>
#include "src/utils/PAnels/PlaceImagePanel.h"

class PlaceholderModel : public QAbstractListModel {
  Q_OBJECT
 public:
  PlaceholderModel(QObject* parent = nullptr);

  Q_INVOKABLE int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  Q_INVOKABLE int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  Q_INVOKABLE QVariant data(const QModelIndex& index, int role) const override;
  Q_INVOKABLE bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  QHash<int, QByteArray> roleNames() const override;
  void setPlaceHolderData(std::vector<PlaceImageLayer>* data, bool refresh = false);

  PlaceImageLayer& getPlaceImageLayer(const QModelIndex& index);
  PlaceImageLayer& getImageLayer(int index);

  Q_INVOKABLE void onImageEditableStatusChange(int row, bool isChecked);
  Q_INVOKABLE bool isAllSelected();
  Q_INVOKABLE void setAllChecked(bool checked);

  Q_INVOKABLE void onFillModeChanged(int row, int modeIndex);
  Q_INVOKABLE void onAssetTypeChange(int row, int typeIndex);

  Q_SIGNAL void itemSelectChange(int fillModeIndex, int assetModeIndex);

private:
  std::vector<PlaceImageLayer>* placeholderData = nullptr;
  mutable QHash<int, QByteArray> roles;
};
#endif //PLACEHOLDERMODEL_H
