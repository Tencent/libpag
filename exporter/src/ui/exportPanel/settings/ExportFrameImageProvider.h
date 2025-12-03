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

#include <QQuickImageProvider>
#include "utils/AEResource.h"

namespace exporter {

class ExportFrameImageProvider : public QQuickImageProvider {
  Q_OBJECT
 public:
  explicit ExportFrameImageProvider();

  void setAEResource(std::shared_ptr<AEResource> resource);
  void updateFrameImage(pag::Frame frame);
  QString getName();
  QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

 private:
  pag::Frame currentFrame = 0;
  std::shared_ptr<AEResource> resource = nullptr;
  std::list<std::pair<pag::Frame, QImage>> frameImageList = {};
};

}  // namespace exporter
