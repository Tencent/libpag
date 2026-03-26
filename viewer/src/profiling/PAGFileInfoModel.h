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
#include "pag/pag.h"

namespace pag {
class PAGFileInfo {
 public:
  explicit PAGFileInfo(const QString& name, const QString& value = "", const QString& unit = "");

  QString name = "";
  QString value = "";
  QString unit = "";
};

class PAGFileInfoModel : public QAbstractListModel {
  Q_OBJECT
 public:
  enum class PAGFileInfoRoles { NameRole = Qt::UserRole + 1, ValueRole, UnitRole };

  PAGFileInfoModel();
  explicit PAGFileInfoModel(QObject* parent);

  QVariant data(const QModelIndex& index, int role) const override;
  int rowCount(const QModelIndex& parent) const override;
  void setPAGFile(std::shared_ptr<PAGFile> pagFile);

 protected:
  QHash<int, QByteArray> roleNames() const override;

 private:
  std::vector<PAGFileInfo> fileInfos = {};
};

}  // namespace pag
