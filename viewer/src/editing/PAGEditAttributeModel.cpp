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

#include "PAGEditAttributeModel.h"
#include <PAGRttr.hpp>
#include <QString>
#include "utils/FileUtils.h"

namespace pag {

PAGEditAttributeModel::PAGEditAttributeModel(QObject* parent) : QObject(parent) {
}

QString PAGEditAttributeModel::getPAGFilePath() {
  return filePath;
}

QStringList PAGEditAttributeModel::getAttributeOptions(const QString& attributeName) {
  QStringList options;

  if (attributeName == "timeStretchMode") {
    auto enumType = rttr::type::get_by_name("PAGTimeStretchMode");
    if (enumType.is_valid()) {
      auto enumItems = enumType.get_enumeration().get_names();
      for (const auto& item : enumItems) {
        options.append(QString::fromStdString(item.data()));
      }
    }
  }

  return options;
}

bool PAGEditAttributeModel::saveAttribute(int layerId, int markerIndex,
                                          const QString& attributeName,
                                          const QString& attributeValue, const QString& savePath) {
  if (_pagFile == nullptr) {
    return false;
  }

  if (filePath.isEmpty()) {
    return false;
  }

  auto path = savePath.isEmpty() ? filePath : savePath;
  auto file = _pagFile->getFile();

  if (attributeName == "timeStretchMode") {
    QStringList options = getAttributeOptions(attributeName);
    if (options.size() <= 0) {
      return false;
    }
    file->timeStretchMode = static_cast<PAGTimeStretchMode>(options.indexOf(attributeValue));
    return Utils::WriteFileToDisk(file, path);
  }

  auto layer = getLayerFromFile(file, layerId);
  if (layer != nullptr) {
    if (attributeName == "name") {
      layer->name = attributeValue.toStdString();
      return Utils::WriteFileToDisk(file, path);
    }
    if (markerIndex < 0) {
      return false;
    }
    auto marker = layer->markers[markerIndex];
    if (attributeName == "startTime") {
      marker->startTime = attributeValue.toInt();
    } else if (attributeName == "duration") {
      marker->duration = attributeValue.toInt();
    } else if (attributeName == "comment") {
      marker->comment = attributeValue.toStdString();
    }
    return Utils::WriteFileToDisk(file, path);
  }

  auto composition = getCompositionFromFile(file, layerId);
  if (composition != nullptr) {
    if (markerIndex < 0) {
      return false;
    }
    auto marker = composition->audioMarkers[markerIndex];
    if (attributeName == "startTime") {
      marker->startTime = attributeValue.toInt();
    }

    if (attributeName == "duration") {
      marker->duration = attributeValue.toInt();
    }

    if (attributeName == "comment") {
      marker->comment = attributeValue.toStdString();
    }
    return Utils::WriteFileToDisk(file, path);
  }
  return false;
}

void PAGEditAttributeModel::setPAGFile(std::shared_ptr<PAGFile> pagFile) {
  _pagFile = std::move(pagFile);
  filePath = QString::fromStdString(_pagFile->path());
}

Layer* PAGEditAttributeModel::getLayerFromFile(std::shared_ptr<File> file, int layerId) {
  if (file == nullptr) {
    return nullptr;
  }

  auto pLayer = getLayerFromPreComposition(layerId, file->getRootLayer());
  if (pLayer != nullptr) {
    return pLayer;
  }

  for (auto& composition : file->compositions) {
    if (composition->type() == CompositionType::Vector) {
      auto vectorComposition = static_cast<VectorComposition*>(composition);
      for (auto& layer : vectorComposition->layers) {
        if (layer->id == static_cast<ID>(layerId)) {
          return layer;
        }

        if (layer->type() == LayerType::PreCompose) {
          auto preComposition = static_cast<PreComposeLayer*>(layer);
          pLayer = getLayerFromPreComposition(layerId, preComposition);
          if (pLayer != nullptr) {
            return pLayer;
          }
        }
      }
    }
  }

  return nullptr;
}

Composition* PAGEditAttributeModel::getCompositionFromFile(std::shared_ptr<File> file,
                                                           int layerId) {
  if (file == nullptr) {
    return nullptr;
  }

  for (auto& composition : file->compositions) {
    if (composition->id == static_cast<ID>(layerId)) {
      return composition;
    }

    if (composition->type() == CompositionType::Vector) {
      auto vectorComposition = static_cast<VectorComposition*>(composition);
      for (auto& layer : vectorComposition->layers) {
        if (layer->type() == LayerType::PreCompose) {
          auto preComposition = static_cast<PreComposeLayer*>(layer);
          auto pLayer = getCompositionFromPreComposition(layerId, preComposition);
          if (pLayer != nullptr) {
            return pLayer;
          }
        }
      }
    }
  }

  return nullptr;
}

Layer* PAGEditAttributeModel::getLayerFromPreComposition(int layerId,
                                                         PreComposeLayer* preComposeLayer) {
  if (preComposeLayer == nullptr) {
    return nullptr;
  }

  if (preComposeLayer->composition->type() == CompositionType::Vector) {
    auto vectorComposition = static_cast<VectorComposition*>(preComposeLayer->composition);
    for (auto& layer : vectorComposition->layers) {
      if (layer->id == static_cast<ID>(layerId)) {
        return layer;
      }

      if (layer->type() == pag::LayerType::PreCompose) {
        auto preComposition = static_cast<PreComposeLayer*>(layer);
        auto pLayer = getLayerFromPreComposition(layerId, preComposition);
        if (pLayer != nullptr) {
          return pLayer;
        }
      }
    }
  }

  return nullptr;
}

Composition* PAGEditAttributeModel::getCompositionFromPreComposition(
    int layerId, PreComposeLayer* preComposeLayer) {
  if (preComposeLayer == nullptr) {
    return nullptr;
  }

  if (preComposeLayer->composition->id == static_cast<ID>(layerId)) {
    return preComposeLayer->composition;
  }

  if (preComposeLayer->composition->type() == CompositionType::Vector) {
    auto vectorComposition = static_cast<VectorComposition*>(preComposeLayer->composition);
    for (auto& layer : vectorComposition->layers) {
      if (layer->type() == pag::LayerType::PreCompose) {
        auto preComposition = static_cast<PreComposeLayer*>(layer);
        auto pLayer = getCompositionFromPreComposition(layerId, preComposition);
        if (pLayer != nullptr) {
          return pLayer;
        }
      }
    }
  }

  return nullptr;
}

}  // namespace pag