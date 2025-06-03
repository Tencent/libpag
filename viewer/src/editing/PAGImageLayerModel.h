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

#pragma once

#include <QAbstractListModel>
#include "pag/pag.h"

namespace pag {

class PAGImageLayerModel : public QAbstractListModel {
  Q_OBJECT
 public:
  enum class PAGImageLayerRoles { IndexRole = Qt::UserRole + 1, ReveryRole };

  explicit PAGImageLayerModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent) const Q_DECL_OVERRIDE;
  QVariant data(const QModelIndex& index, int role) const Q_DECL_OVERRIDE;
  QImage requestImage(int index);

  Q_SLOT void setFile(const std::shared_ptr<PAGFile>& pagFile, const std::string& filePath);

  Q_INVOKABLE void replaceImage(int index, const QString& filePath);
  Q_INVOKABLE void revertImage(int index);
  Q_INVOKABLE int convertIndex(int index);

 protected:
  QHash<int, QByteArray> roleNames() const Q_DECL_OVERRIDE;

 private:
  QSet<int> revertSet = {};
  QMap<int, QImage> imageLayers = {};
  std::shared_ptr<PAGFile> pagFile = nullptr;
};

}  // namespace pag