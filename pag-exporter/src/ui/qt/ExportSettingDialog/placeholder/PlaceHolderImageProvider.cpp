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
#include "PlaceHolderImageProvider.h"
#include <QtCore/QDebug>

static int ICON_SIZE = 48;

PlaceHolderImageProvider::PlaceHolderImageProvider(std::vector<PlaceImageLayer>* placeImageLayers)
    : placeImageLayers(placeImageLayers), QQuickImageProvider(QQuickImageProvider::Image) {
}

void PlaceHolderImageProvider::resetLayers(std::vector<PlaceImageLayer>* placeImageLayers) {
  this->placeImageLayers = placeImageLayers;
}

QImage PlaceHolderImageProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize) {
  int row = id.split("/").first().toInt();
  //  qDebug() << "requestImage id:" << id;
  if (row < 0 || row >= placeImageLayers->size()) {
    qDebug() << "requestImage row invalid";
    return QImage();
  }
  ImageRgbaData& rgbaData = placeImageLayers->at(row).thumbnail;
  if (rgbaData.data == nullptr) {
    qDebug() << "requestImage rgbaData invalid";
    return QImage();
  }
  QImage img(rgbaData.data.get(), ICON_SIZE, ICON_SIZE, QImage::Format_RGBA8888);
  return img;
}