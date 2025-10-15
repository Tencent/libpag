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

#include <QAbstractItemModel>
#include "editing/serialize/PAGTree.h"
#include "pag/pag.h"

namespace pag {

class PAGTreeViewModel : public QAbstractItemModel {
  Q_OBJECT
 public:
  enum class PAGTreeViewModelRoles {
    NameRole = Qt::UserRole + 1,
    ValueRole,
    IsEditableKeyRole,
    LayerIdKeyRole,
    MarkerIndexKeyRole,
  };

  explicit PAGTreeViewModel(QObject* parent = nullptr);

  QVariant data(const QModelIndex& index, int role) const Q_DECL_OVERRIDE;
  bool setData(const QModelIndex& index, const QVariant& value, int role) Q_DECL_OVERRIDE;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const Q_DECL_OVERRIDE;
  bool setHeaderData(int section, Qt::Orientation orientation, const QVariant& value,
                     int role) Q_DECL_OVERRIDE;
  Qt::ItemFlags flags(const QModelIndex& index) const Q_DECL_OVERRIDE;
  QModelIndex index(int row, int column, const QModelIndex& parent) const Q_DECL_OVERRIDE;
  QModelIndex parent(const QModelIndex& index) const Q_DECL_OVERRIDE;
  int rowCount(const QModelIndex& parent) const Q_DECL_OVERRIDE;
  int columnCount(const QModelIndex& parent) const Q_DECL_OVERRIDE;
  QHash<int, QByteArray> roleNames() const Q_DECL_OVERRIDE;

  Q_SLOT void setFile(std::shared_ptr<File> file);

 private:
  std::unique_ptr<PAGTree> fileTree = nullptr;
};

}  //  namespace pag