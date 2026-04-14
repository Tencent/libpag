/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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
#include <QString>
#include "pagx/PAGXDocument.h"

namespace pag {

class PAGNodeStatItem {
 public:
  PAGNodeStatItem(const QString& name = "", const QString& color = "", int count = 0);

  QString name = "";
  QString color = "";
  int count = 0;
};

class PAGNodeStatsModel : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int totalCount READ getTotalCount NOTIFY totalCountChanged)
  Q_PROPERTY(int count READ getCount NOTIFY countChanged)
 public:
  enum class PAGNodeStatsRoles { NameRole = Qt::UserRole + 1, ColorRole, CountRole };

  explicit PAGNodeStatsModel(QObject* parent = nullptr);

  QVariant data(const QModelIndex& index, int role) const override;
  int rowCount(const QModelIndex& parent) const override;
  int getTotalCount() const;
  int getCount() const;

  void setPAGXDocument(std::shared_ptr<pagx::PAGXDocument> pagxDocument);

  Q_SIGNAL void totalCountChanged();
  Q_SIGNAL void countChanged();

 protected:
  QHash<int, QByteArray> roleNames() const override;

 private:
  std::vector<PAGNodeStatItem> statItems = {};
  int totalCount = 0;
};

}  // namespace pag
