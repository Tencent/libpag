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

class PAGImageLayerModel : public QAbstractListModel {
  Q_OBJECT
 public:
  enum class PAGImageLayerRoles { IndexRole = Qt::UserRole + 1, ReveryRole };

  static QImage GetVideoFirstFrame(const QString& filePath);

  explicit PAGImageLayerModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent) const Q_DECL_OVERRIDE;
  QVariant data(const QModelIndex& index, int role) const Q_DECL_OVERRIDE;
  QImage requestImage(int index);

  Q_SIGNAL void imageChanged();

  Q_SLOT void setPAGFile(std::shared_ptr<PAGFile> pagFile);

  Q_INVOKABLE void changeImage(int index, const QString& filePath);
  Q_INVOKABLE void revertImage(int index);
  Q_INVOKABLE int convertIndex(int index);

 protected:
  QHash<int, QByteArray> roleNames() const Q_DECL_OVERRIDE;

 private:
  void replaceImage(int index, std::shared_ptr<PAGImage> image);

  QSet<int> revertSet = {};
  QMap<int, QImage> imageLayers = {};
  std::shared_ptr<PAGFile> _pagFile = nullptr;
};

}  // namespace pag
