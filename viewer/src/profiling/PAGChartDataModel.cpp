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

#include "PAGChartDataModel.h"

namespace pag {

QString PAGCharDataItem::getRenderTime() {
  return QString::number(renderTime);
}

QString PAGCharDataItem::getPresentTime() {
  return QString::number(presentTime);
}

QString PAGCharDataItem::getImageDecodeTime() {
  return QString::number(imageDecodeTime);
}

PAGChartDataModel::~PAGChartDataModel() {
  clearItems();
}

int PAGChartDataModel::getSize() const {
  return static_cast<int>(items.size());
}

int PAGChartDataModel::getCurrentIndex() const {
  return currentIndex;
}

QString PAGChartDataModel::getMaxTime() const {
  return QString::number(maxTime);
}

QQmlListProperty<PAGCharDataItem> PAGChartDataModel::getItems() {
  return QQmlListProperty<PAGCharDataItem>(this, this, &PAGChartDataModel::AppendItem,
                                           &PAGChartDataModel::Count, &PAGChartDataModel::GetItem,
                                           &PAGChartDataModel::ClearItem);
}

void PAGChartDataModel::addItem(PAGCharDataItem* item) {
  auto* newItem = new PAGCharDataItem();
  newItem->renderTime = item->renderTime;
  newItem->presentTime = item->presentTime;
  newItem->imageDecodeTime = item->imageDecodeTime;
  items.append(newItem);
  int64_t sum = newItem->renderTime + newItem->presentTime + newItem->imageDecodeTime;
  maxTime = sum > maxTime ? sum : maxTime;
  currentIndex = static_cast<int>(items.size() - 1);
  Q_EMIT itemsChange();
}

PAGCharDataItem* PAGChartDataModel::getItem(int index) {
  if (index < 0 || index >= items.size()) {
    return nullptr;
  }
  return items[index];
}

void PAGChartDataModel::updateOrInsertItem(int index, PAGCharDataItem* item) {
  if (index > (items.size() - 1)) {
    addItem(item);
    return;
  }
  items[index]->renderTime = item->renderTime;
  items[index]->presentTime = item->presentTime;
  items[index]->imageDecodeTime = item->imageDecodeTime;
  int64_t max = 0;
  for (auto& it : items) {
    int64_t sum = it->renderTime + it->presentTime + it->imageDecodeTime;
    max = sum > max ? sum : max;
  }
  maxTime = max;
  currentIndex = index;
  Q_EMIT itemsChange();
}

void PAGChartDataModel::clearItems() {
  qDeleteAll(items);
  items.clear();
  currentIndex = -1;
  maxTime = 0;
  Q_EMIT itemsChange();
}

void PAGChartDataModel::resetItems(PAGChartDataModel* model) {
  clearItems();
  for (auto& item : model->items) {
    addItem(item);
  }
  maxTime = model->maxTime;
  currentIndex = model->currentIndex;
  Q_EMIT itemsChange();
}

void PAGChartDataModel::AppendItem(QQmlListProperty<PAGCharDataItem>* list, PAGCharDataItem* item) {
  static_cast<PAGChartDataModel*>(list->data)->addItem(item);
}

void PAGChartDataModel::ClearItem(QQmlListProperty<PAGCharDataItem>* list) {
  static_cast<PAGChartDataModel*>(list->data)->clearItems();
}

PAGCharDataItem* PAGChartDataModel::GetItem(QQmlListProperty<PAGCharDataItem>* list, qsizetype i) {
  return static_cast<PAGChartDataModel*>(list->data)->getItem(static_cast<int>(i));
}

qsizetype PAGChartDataModel::Count(QQmlListProperty<PAGCharDataItem>* list) {
  return static_cast<PAGChartDataModel*>(list->data)->getSize();
}

}  // namespace pag
