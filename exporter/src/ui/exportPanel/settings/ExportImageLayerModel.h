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

#pragma once

#include <QAbstractListModel>
#include "ExportCompositionInfoModel.h"
#include "utils/AEResource.h"

namespace exporter {

class ExportImageLayerModel : public QAbstractListModel {
  Q_OBJECT
 public:
  enum class ExportImageLayerModelRoles {
    NumberRole = Qt::UserRole + 1,
    NameRole,
    FillModeRole,
    IsEditableRole
  };

  explicit ExportImageLayerModel(GetSessionHandler getSessionHandler, QObject* parent = nullptr);

  Q_PROPERTY(bool isAllEditable READ getAllEditable NOTIFY allEditableChanged WRITE setAllEditable)

  void setAEResource(std::shared_ptr<AEResource> resource);
  void refreshData(std::shared_ptr<AEResource> resource);

  Q_INVOKABLE void setIsEditable(int row, bool isEditable);
  Q_INVOKABLE void setAllEditable(bool allEditable);
  Q_INVOKABLE void setScaleMode(int row, const QString& scaleMode);
  Q_INVOKABLE bool getAllEditable() const;
  Q_INVOKABLE QStringList getScaleModes() const;

  int rowCount(const QModelIndex& parent) const override;
  QVariant data(const QModelIndex& index, int role) const override;

  Q_SIGNAL void allEditableChanged(bool allEditable);

  Q_SLOT void onCompositionExportAsBmpChanged();

 protected:
  QHash<int, QByteArray> roleNames() const override;

 private:
  size_t editableItemNum = 0;
  std::shared_ptr<AEResource> resource = nullptr;
  std::vector<AEResource::Layer> items = {};
  GetSessionHandler getSessionHandler = nullptr;
};

}  // namespace exporter
