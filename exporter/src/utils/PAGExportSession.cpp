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

#include "PAGExportSession.h"
#include "AEDataTypeConverter.h"
#include "AEHelper.h"
#include "AEPReader.h"
#include "ByteArray.h"
#include "StringHelper.h"
#include "config/ConfigFile.h"
#include "src/base/utils/Log.h"

namespace exporter {

PAGExportSession::PAGExportSession(const AEGP_ItemH& activeItemHandle,
                                   const std::string& outputPath)
    : itemHandle(activeItemHandle), outputPath(outputPath) {
  checkParamValid();
}

void PAGExportSession::checkParamValid() {
  const ConfigParam tempParam;

  ReadConfigFile(&configParam);
  configParam.frameRate = std::clamp(configParam.frameRate, 0.01f, 120.0f);

  switch (configParam.tagMode) {
    case TagMode::Beta:
      configParam.exportTagLevel = static_cast<uint16_t>(PresetTagLevel::TagLevelMax);
      break;
    case TagMode::Stable:
      configParam.exportTagLevel = static_cast<uint16_t>(PresetTagLevel::TagLevelStable);
      break;
    case TagMode::Custom:
      // Custom mode doesn't modify exportTagLevel
      break;
    default:
      LOGI("Warning! unsupported tagMode: %d", static_cast<int>(configParam.tagMode));
      configParam.tagMode = TagMode::Stable;
      configParam.exportTagLevel = static_cast<uint16_t>(PresetTagLevel::TagLevelStable);
      break;
  }

  configParam.exportTagLevel =
      std::clamp(configParam.exportTagLevel, static_cast<uint16_t>(PresetTagLevel::TagLevelMin),
                 static_cast<uint16_t>(PresetTagLevel::TagLevelMax));
  configParam.imageQuality = std::clamp(configParam.imageQuality, 0, 100);
  configParam.imagePixelRatio = std::clamp(configParam.imagePixelRatio, 1.0f, 3.0f);

  if (configParam.sequenceType != pag::CompositionType::Video &&
      configParam.sequenceType != pag::CompositionType::Bitmap) {
    configParam.sequenceType = tempParam.sequenceType;
  }

  configParam.sequenceQuality = std::clamp(configParam.sequenceQuality, 0, 100);
}

void PAGExportSession::pushWarning(AlertInfoType type, const std::string& addInfo) {
  AEGP_ItemH itemHandle = nullptr;
  if (itemHandleMap.find(compID) != itemHandleMap.end()) {
    itemHandle = itemHandleMap[compID];
  }
  AEGP_LayerH layerHandle = nullptr;
  if (layerHandleMap.find(layerID) != layerHandleMap.end()) {
    layerHandle = layerHandleMap[layerID];
  }
  alertInfoManager.pushWarning(itemHandle, layerHandle, type, addInfo);
}

pag::GradientColorHandle PAGExportSession::GetGradientColorsFromFileBytes(
    const std::vector<std::string>& matchNames, int index, int keyFrameIndex) {
  if (fileBytes.empty()) {
    fileBytes = GetProjectFileBytes();
  }
  if (fileBytes.empty()) {
    return GetDefaultGradientColors();
  }

  ByteArray bytes(reinterpret_cast<uint8_t*>(fileBytes.data()), fileBytes.size());
  bytes = ReadBody(&bytes);
  auto compositions = ReadCompositions(&bytes);

  ByteArray layerBytes = {};
  for (auto& composition : compositions) {
    if (composition.id == static_cast<int>(compID)) {
      auto layers = ReadLayers(&composition.bytes);
      for (const auto& layer : layers) {
        if (static_cast<pag::ID>(layer.id) == layerID) {
          layerBytes = layer.bytes;
          break;
        }
      }
      break;
    }
  }

  if (layerBytes.empty()) {
    return GetDefaultGradientColors();
  }

  std::string gradientText;
  for (int i = 0; layerBytes.bytesAvailable() > 0 && i <= index; ++i) {
    auto groupTag = ReadFirstGroupByMatchNames(&layerBytes, matchNames);
    if (groupTag.bytes.empty()) {
      break;
    }

    if (i == index) {
      auto tag = ReadFirstTagByName(&groupTag.bytes, "GCky");
      if (tag.bytes.empty()) {
        break;
      }

      int frameIndex = 0;
      while (tag.bytes.bytesAvailable()) {
        auto stringTag = ReadTag(&tag.bytes);
        if (stringTag.bytes.empty()) {
          break;
        }
        if (frameIndex == keyFrameIndex) {
          if (!stringTag.bytes.empty() && stringTag.bytes.bytesAvailable() > 0) {
            if (stringTag.bytes.position() < stringTag.bytes.length()) {
              gradientText = stringTag.bytes.readUTF8String();
            }
          }
          break;
        }
        frameIndex++;
      }
      break;
    }
  }

  return XmlToGradientColor(gradientText);
}

bool PAGExportSession::isVideoLayer(pag::ID id) {
  if (layerHandleMap.find(id) == layerHandleMap.end()) {
    return false;
  }
  AEGP_LayerH layerHandle = layerHandleMap[id];

  for (const auto& pair : imageLayerHandleList) {
    if (pair.second == layerHandle) {
      return pair.first;
    }
  }

  return false;
}

AEGP_LayerH PAGExportSession::getLayerHByID(pag::ID id) {
  if (layerHandleMap.find(id) == layerHandleMap.end()) {
    return nullptr;
  }
  return layerHandleMap[id];
}

}  // namespace exporter
