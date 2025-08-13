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

#include <QObject>
#include <QQmlListProperty>

namespace pag {

class PAGCharDataItem : public QObject {
  Q_OBJECT
 public:
  Q_PROPERTY(QString renderTime READ getRenderTime)
  Q_PROPERTY(QString presentTime READ getPresentTime)
  Q_PROPERTY(QString imageDecodeTime READ getImageDecodeTime)
  QString getRenderTime();
  QString getPresentTime();
  QString getImageDecodeTime();

  int64_t renderTime = 0;
  int64_t presentTime = 0;
  int64_t imageDecodeTime = 0;
};

class PAGChartDataModel : public QObject {
  Q_OBJECT
 public:
  ~PAGChartDataModel() override;

  Q_PROPERTY(int size READ getSize)
  Q_PROPERTY(int currentIndex READ getCurrentIndex)
  Q_PROPERTY(QString maxTime READ getMaxTime)
  Q_PROPERTY(QQmlListProperty<PAGCharDataItem> items READ getItems NOTIFY itemsChange)

  Q_SIGNAL void itemsChange();

  int getSize() const;
  int getCurrentIndex() const;
  QString getMaxTime() const;
  QQmlListProperty<PAGCharDataItem> getItems();

  void addItem(PAGCharDataItem* item);
  PAGCharDataItem* getItem(int index);
  void updateOrInsertItem(int index, PAGCharDataItem* item);

  void clearItems();
  void resetItems(PAGChartDataModel* model);

 private:
  static void AppendItem(QQmlListProperty<PAGCharDataItem>* list, PAGCharDataItem* item);
  static void ClearItem(QQmlListProperty<PAGCharDataItem>* list);
  static PAGCharDataItem* GetItem(QQmlListProperty<PAGCharDataItem>* list, qsizetype i);
  static qsizetype Count(QQmlListProperty<PAGCharDataItem>* list);

  int currentIndex = -1;
  int64_t maxTime = 0;
  QVector<PAGCharDataItem*> items = {};
};

}  // namespace pag
