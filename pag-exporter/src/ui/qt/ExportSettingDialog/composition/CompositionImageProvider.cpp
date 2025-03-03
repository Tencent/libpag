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
#include "CompositionImageProvider.h"
#include <QtCore/QDebug>

CompositionImageProvider::CompositionImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {
}

CompositionImageProvider::~CompositionImageProvider() noexcept {
  auto beginIter = snapshotCachedMap.begin();
  while (beginIter != snapshotCachedMap.end()) {
    auto image = beginIter->second;
    delete image;
    beginIter++;
  }
  snapshotCachedMap.clear();
  qDebug() << "CompositionImageProvider release";
}

bool CompositionImageProvider::isCacheFrame(const QString& frameId) {
  return snapshotCachedMap.find(frameId) != snapshotCachedMap.end();
}

void CompositionImageProvider::cacheFrame(const QString& frameId, QImage* image) {
  snapshotCachedMap.insert(std::pair<QString, QImage*>(frameId, image));
}

QImage CompositionImageProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize) {
  if (!isCacheFrame(id)) {
    return QImage();
  }
  QImage *imageData = snapshotCachedMap[id];
  qDebug() << "requestImage id:" << id << ", imageData:" << imageData;
  return *imageData;
}
