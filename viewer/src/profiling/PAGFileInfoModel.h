/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

  auto data(const QModelIndex& index, int role) const -> QVariant override;
  auto rowCount(const QModelIndex& parent) const -> int override;
  auto resetFile(const std::shared_ptr<PAGFile>& pagFile, const std::string& filePath) -> void;

 protected:
  auto roleNames() const -> QHash<int, QByteArray> override;

 private:
  std::vector<PAGFileInfo> fileInfos = {};
};

}  // namespace pag