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

#include "ExportFrameImageProvider.h"
#include "utils/AEHelper.h"

namespace exporter {

ExportFrameImageProvider::ExportFrameImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image) {
}

void ExportFrameImageProvider::setAEResource(std::shared_ptr<AEResource> newResource) {
  this->resource = std::move(newResource);
  updateFrameImage(0);
}

void ExportFrameImageProvider::updateFrameImage(pag::Frame frame) {
  auto iter = std::find_if(frameImageList.begin(), frameImageList.end(),
                           [frame](auto& pair) { return pair.first == frame; });
  if (iter != frameImageList.end()) {
    return;
  }
  QImage image = GetCompositionFrameImage(resource->itemHandle, frame);
  frameImageList.emplace_back(frame, image);
  if (frameImageList.size() > 5) {
    frameImageList.pop_front();
  }
}

QString ExportFrameImageProvider::getName() {
  return QString("PAGExporterFrameImageProvider_") +
         QString::number(reinterpret_cast<quintptr>(this));
}

QImage ExportFrameImageProvider::requestImage(const QString& id, QSize*, const QSize&) {
  pag::Frame theID = id.toLongLong();
  auto iter = std::find_if(frameImageList.begin(), frameImageList.end(),
                           [theID](auto& pair) { return pair.first == theID; });
  if (iter != frameImageList.end()) {
    QImage image = iter->second;
    return image;
  }
  return {};
}

}  // namespace exporter
