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
#ifndef COMPOSITIONMODEL_H
#define COMPOSITIONMODEL_H
#include <QtCore/QAbstractListModel>
#include "src/ui/qt/ExportCommonConfig.h"
#include "src/ui/qt//common/CommonAlertDialog.h"
#include "src/utils/Panels/AECompositionPanel.h"

class CompositionModel : public QAbstractListModel {
  Q_OBJECT
 public:
  explicit CompositionModel(std::shared_ptr<AECompositionPanel> compositionPanel = nullptr, QObject* parent = nullptr);
  ~CompositionModel() override = default;
  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  Q_INVOKABLE bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
  QHash<int, QByteArray> roleNames() const override;

  void setCompositionData(std::vector<AEComposition>* compositionData);
  Q_SIGNAL void bmpDataChange();

  Q_INVOKABLE void onLayerBmpStatusChange(int row, bool isChecked);
  Q_INVOKABLE int getSliderMaxValue();
  Q_INVOKABLE QString getCompositionName();

private:
  std::shared_ptr<AECompositionPanel> compositionPanel;
  std::vector<AEComposition>* compositionVector;
  mutable QHash<int, QByteArray> roles;
};
#endif //COMPOSITIONMODEL_H
