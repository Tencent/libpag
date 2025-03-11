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
#ifndef LAYEREDITABLEMODEL_H
#define LAYEREDITABLEMODEL_H
#include <QtCore/QAbstractListModel>
#include "src/utils/Panels/TextLayerEditablePanel.h"

class LayerEditableModel : public QAbstractListModel {
  Q_OBJECT
 public:
  explicit LayerEditableModel(QObject* parent = nullptr);
  ~LayerEditableModel() override = default;

  Q_INVOKABLE int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  Q_INVOKABLE int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  Q_INVOKABLE QVariant data(const QModelIndex& index, int role) const override;
  Q_INVOKABLE bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  QHash<int, QByteArray> roleNames() const override;
  void setPlaceHolderData(std::vector<PlaceTextLayer>* data, bool refresh = false);

  PlaceTextLayer& getPlaceTextLayer(const QModelIndex& index);

  Q_INVOKABLE void onLayerEditableStatusChange(int row, bool isChecked);
  Q_INVOKABLE bool isAllSelected();
  Q_INVOKABLE void setAllChecked(bool checked);

private:
  std::vector<PlaceTextLayer>* placeTextData = nullptr;
  mutable QHash<int, QByteArray> roles;
};
#endif //LAYEREDITABLEMODEL_H
