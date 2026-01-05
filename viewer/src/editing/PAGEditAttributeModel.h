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
#include "pag/pag.h"

namespace pag {

class PAGEditAttributeModel : public QObject {
  Q_OBJECT
 public:
  explicit PAGEditAttributeModel(QObject* parent = nullptr);

  Q_INVOKABLE QString getPAGFilePath();
  Q_INVOKABLE QStringList getAttributeOptions(const QString& attributeName);
  Q_INVOKABLE bool saveAttribute(int layerId, int markerIndex, const QString& attributeName,
                                 const QString& attributeValue, const QString& savePath = "");

  Q_SLOT void setPAGFile(std::shared_ptr<PAGFile> pagFile);

 private:
  Layer* getLayerFromFile(std::shared_ptr<File> file, int layerId);
  Composition* getCompositionFromFile(std::shared_ptr<File> file, int layerId);
  Layer* getLayerFromPreComposition(int layerId, PreComposeLayer* preComposeLayer);
  Composition* getCompositionFromPreComposition(int layerId, PreComposeLayer* preComposeLayer);

 private:
  QString filePath = "";
  std::shared_ptr<PAGFile> _pagFile = nullptr;
};

}  // namespace pag
