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

#include "PAGExportSession.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include "AEHelper.h"
#include "AEPReader.h"
#include "ByteArray.h"
#include "FileHelper.h"
#include "PsdTextAttribute.h"
#include "StringHelper.h"
#include "platform/PlatformHelper.h"

namespace fs = std::filesystem;
using namespace StringHelper;

namespace exporter {

PAGExportSession::PAGExportSession(std::string& path)
    : pluginID(AEHelper::GetPluginID()), suites(AEHelper::GetSuites()), outputPath(path),
      bEarlyExit(false) {
  checkParamValid();
}

PAGExportSession::~PAGExportSession() {
}

void PAGExportSession::checkParamValid() {
  const ConfigParam tempParam;

  if (configParam.frameRate > 120) {
    configParam.frameRate = 120;
  } else if (configParam.frameRate < 0.01) {
    configParam.frameRate = 0.01;
  }

  switch (configParam.tagMode) {
    case TagMode::Beta:
      configParam.exportTagLevel = static_cast<uint16_t>(PresetTagLevel::TagLevelMax);
      break;
    case TagMode::Stable:
      configParam.exportTagLevel = static_cast<uint16_t>(PresetTagLevel::TagLevelStable);
      break;
    case TagMode::Custom:
      break;
    default:
      std::cerr << "Error! unsupported tagMode:" << static_cast<int>(configParam.tagMode)
                << std::endl;
      configParam.tagMode = TagMode::Stable;
      configParam.exportTagLevel = static_cast<uint16_t>(PresetTagLevel::TagLevelStable);
      break;
  }

  if (configParam.exportTagLevel < static_cast<uint16_t>(PresetTagLevel::TagLevelMin)) {
    configParam.exportTagLevel = static_cast<uint16_t>(PresetTagLevel::TagLevelMin);
  } else if (configParam.exportTagLevel > static_cast<uint16_t>(PresetTagLevel::TagLevelMax)) {
    configParam.exportTagLevel = static_cast<uint16_t>(PresetTagLevel::TagLevelMax);
  }

  if (configParam.imageQuality < 0) {
    configParam.imageQuality = 0;
  } else if (configParam.imageQuality > 100) {
    configParam.imageQuality = 100;
  }

  if (configParam.imagePixelRatio < 1.0) {
    configParam.imagePixelRatio = 1.0;
  } else if (configParam.imagePixelRatio > 3.0) {
    configParam.imagePixelRatio = 3.0;
  }

  transform(configParam.sequenceSuffix.begin(), configParam.sequenceSuffix.end(),
            configParam.sequenceSuffix.begin(), ::tolower);
  if (configParam.sequenceSuffix.empty()) {
    configParam.sequenceSuffix = tempParam.sequenceSuffix;
  }
  if (configParam.sequenceType != pag::CompositionType::Video &&
      configParam.sequenceType != pag::CompositionType::Bitmap) {
    configParam.sequenceType = tempParam.sequenceType;
  }

  if (configParam.sequenceQuality < 0) {
    configParam.sequenceQuality = 0;
  } else if (configParam.sequenceQuality > 100) {
    configParam.sequenceQuality = 100;
  }
}

pag::TextDocumentHandle PAGExportSession::currentTextDocument() {
  auto id = std::to_string(curCompId);
  auto layer = std::to_string(layerIndex);
  auto keyframe = std::to_string(keyframeIndex);
  std::string result = "";
  if (enableRunScript) {
    AEHelper::RegisterTextDocumentScript();
    auto code = "PAG.printTextDocuments(" + id + ", " + layer + ", " + keyframe + ");";
    result = AEHelper::RunScript(suites, pluginID, code);
  }
  if (result.empty()) {
    return std::make_shared<pag::TextDocument>();
  }
  std::unordered_map<std::string, std::string> valueMap;
  auto lines = StringHelper::Split(result, "\n");
  for (auto& line : lines) {
    auto index = line.find(" : ");
    if (index == std::string::npos) {
      continue;
    }
    auto key = line.substr(0, index);
    auto value = line.substr(index + 3);
    valueMap.insert(std::make_pair(key, value));
  }
  auto textDocument = std::make_shared<pag::TextDocument>();
  textDocument->direction = currentTextDocumentDirection();
  textDocument->applyFill =
      StringToBoolean(GetMapValue(valueMap, "applyFill"), textDocument->applyFill);
  textDocument->applyStroke =
      StringToBoolean(GetMapValue(valueMap, "applyStroke"), textDocument->applyStroke);
  textDocument->baselineShift =
      StringToFloat(GetMapValue(valueMap, "baselineShift"), textDocument->baselineShift);
  textDocument->boxText = StringToBoolean(GetMapValue(valueMap, "boxText"), textDocument->boxText);
  textDocument->boxTextPos =
      StringToPoint(GetMapValue(valueMap, "boxTextPos"), textDocument->boxTextPos);
  textDocument->boxTextSize =
      StringToPoint(GetMapValue(valueMap, "boxTextSize"), textDocument->boxTextSize);
  textDocument->fauxBold =
      StringToBoolean(GetMapValue(valueMap, "fauxBold"), textDocument->fauxBold);
  textDocument->fauxItalic =
      StringToBoolean(GetMapValue(valueMap, "fauxItalic"), textDocument->fauxItalic);
  textDocument->fillColor =
      StringToColor(GetMapValue(valueMap, "fillColor"), textDocument->fillColor);
  textDocument->fontFamily = FormatString(GetMapValue(valueMap, "fontFamily"), "");
  textDocument->fontStyle = FormatString(GetMapValue(valueMap, "fontStyle"), "");
  textDocument->fontSize = StringToFloat(GetMapValue(valueMap, "fontSize"), textDocument->fontSize);
  textDocument->strokeColor =
      StringToColor(GetMapValue(valueMap, "strokeColor"), textDocument->strokeColor);
  textDocument->strokeOverFill =
      StringToBoolean(GetMapValue(valueMap, "strokeOverFill"), textDocument->strokeOverFill);
  textDocument->strokeWidth =
      StringToFloat(GetMapValue(valueMap, "strokeWidth"), textDocument->strokeWidth);
  textDocument->text = FormatString(GetMapValue(valueMap, "text"), textDocument->text);
  textDocument->justification =
      StringToEnum(GetMapValue(valueMap, "justification"), textDocument->justification);
  textDocument->leading = StringToFloat(GetMapValue(valueMap, "leading"), textDocument->leading);
  if (textDocument->leading == 0) {
    textDocument->leading =
        CalculateLineSpacing(GetMapValue(valueMap, "baselineLocs"), textDocument->fontSize);
  }
  textDocument->tracking = StringToFloat(GetMapValue(valueMap, "tracking"), textDocument->tracking);
  auto lineHeight =
      textDocument->leading == 0 ? roundf(textDocument->fontSize * 1.2f) : textDocument->leading;
  textDocument->firstBaseLine = CalculateFirstBaseline(
      GetMapValue(valueMap, "baselineLocs"), lineHeight, textDocument->baselineShift,
      textDocument->direction == pag::TextDirection::Vertical);

  auto pathIndex = this->outputPath.find_last_of('/');
  auto path = this->outputPath.substr(0, pathIndex);
  path += "/" + textDocument->fontFamily + "-" + textDocument->fontStyle + ".ttc";
  fontFilePathList.push_back(path);
  auto fontLocation = FormatString(GetMapValue(valueMap, "fontLocation"), "");

  FileHelper::CopyFile(fontLocation, path);
  int size = FileHelper::GetFileSize(path);
  if (size > 30 * 1000 * 1000) {  // 30MB
    pushWarning(AlertInfoType::FontFileTooBig,
                textDocument->fontFamily + ", " + std::to_string(size / 1000 / 1000) + "MB");
  }

  return textDocument;
}

std::vector<float> PAGExportSession::ParseFloats(const std::string& text) {
  std::vector<float> list;
  auto remaining = text;
  while (!remaining.empty()) {
    auto index = remaining.find("<float>");
    if (index == std::string::npos) {
      break;
    }
    remaining = remaining.substr(index + 7, remaining.size() - index - 7);
    index = remaining.find("</float>");
    if (index == std::string::npos || index == 0) {
      break;
    }
    auto floatText = remaining.substr(0, index);
    remaining = remaining.substr(index + 7, remaining.size() - index - 7);
    auto value = std::stof(floatText);
    list.push_back(value);
  }
  return list;
}

std::vector<pag::AlphaStop> PAGExportSession::ParseAlphaStops(const std::string& text) {
  std::vector<pag::AlphaStop> list;
  auto remaining = text;
  while (!remaining.empty()) {
    auto index = remaining.find("<key>Stops Alpha</key>");
    if (index == std::string::npos) {
      break;
    }
    remaining = remaining.substr(index, remaining.size() - index);
    index = remaining.find("</array>");
    if (index == std::string::npos) {
      break;
    }
    auto alphaText = remaining.substr(0, index);
    remaining = remaining.substr(index, remaining.size() - index);
    auto floats = ParseFloats(alphaText);
    if (floats.size() < 3) {
      break;
    }
    pag::AlphaStop stop = {};
    stop.position = floats[0];
    stop.midpoint = floats[1];
    stop.opacity = static_cast<uint8_t>(floats[2] * 255);
    list.push_back(stop);
  }
  return list;
}

std::vector<pag::ColorStop> PAGExportSession::ParseColorStops(const std::string& text) {
  std::vector<pag::ColorStop> list;
  auto remaining = text;
  while (!remaining.empty()) {
    auto index = remaining.find("<key>Stops Color</key>");
    if (index == std::string::npos) {
      break;
    }
    remaining = remaining.substr(index, remaining.size() - index);
    index = remaining.find("</array>");
    if (index == std::string::npos) {
      break;
    }
    auto colorText = remaining.substr(0, index);
    remaining = remaining.substr(index, remaining.size() - index);
    auto floats = ParseFloats(colorText);
    if (floats.size() < 6) {
      break;
    }
    pag::Color color = {};
    color.red = static_cast<uint8_t>(255 * floats[2]);
    color.green = static_cast<uint8_t>(255 * floats[3]);
    color.blue = static_cast<uint8_t>(255 * floats[4]);
    pag::ColorStop stop = {};
    stop.position = floats[0];
    stop.midpoint = floats[1];
    stop.color = color;
    list.push_back(stop);
  }
  return list;
}

template <typename T>
void PAGExportSession::ResortGradientStops(std::vector<T>& list) {
  std::sort(list.begin(), list.end(),
            [](const T& a, const T& b) { return a.position < b.position; });
}

pag::GradientColorHandle PAGExportSession::DefaultGradientColors() {
  auto gradientColor = std::make_shared<pag::GradientColor>();
  pag::AlphaStop stop = {};
  stop.position = 0.0f;
  gradientColor->alphaStops.push_back(stop);
  stop = {};
  stop.position = 1.0f;
  gradientColor->alphaStops.push_back(stop);
  pag::ColorStop colorStop = {};
  colorStop.position = 0.0f;
  colorStop.color = pag::White;
  gradientColor->colorStops.push_back(colorStop);
  colorStop = {};
  colorStop.position = 1.0f;
  colorStop.color = pag::Black;
  gradientColor->colorStops.push_back(colorStop);
  return gradientColor;
}

pag::GradientColorHandle PAGExportSession::ParseGradientColor(const std::string& text) {
  if (text.empty()) {
    return DefaultGradientColors();
  }
  auto gradientColor = std::make_shared<pag::GradientColor>();
  gradientColor->alphaStops = ParseAlphaStops(text);
  gradientColor->colorStops = ParseColorStops(text);
  if (gradientColor->alphaStops.size() < 2 || gradientColor->colorStops.size() < 2) {
    return DefaultGradientColors();
  }
  ResortGradientStops(gradientColor->alphaStops);
  ResortGradientStops(gradientColor->colorStops);
  return gradientColor;
}

pag::GradientColorHandle PAGExportSession::currentGradientColors(
    const std::vector<std::string>& matchNames, int index) {
  auto fileBytes = getFileBytes();
  if (!fileBytes) {
    return DefaultGradientColors();
  }

  ByteArray bytes(reinterpret_cast<uint8_t*>(fileBytes.get()), static_cast<uint32_t>(fileLength));
  bytes = AEPReader::ReadBody(&bytes);
  auto compositions = AEPReader::ReadCompositions(&bytes);

  ByteArray layerBytes = {};
  for (auto& composition : compositions) {
    if (composition.id == static_cast<int>(curCompId)) {
      auto layers = AEPReader::ReadLayers(&composition.bytes);
      if (layerIndex < static_cast<int>(layers.size())) {
        layerBytes = layers[layerIndex].bytes;
      }
      break;
    }
  }

  if (layerBytes.empty()) {
    return DefaultGradientColors();
  }

  std::string gradientText;
  for (int i = 0; layerBytes.bytesAvailable() > 0 && i <= index; ++i) {
    auto tag = AEPReader::ReadFirstGroupByMatchNames(&layerBytes, matchNames);
    if (tag.bytes.empty()) {
      break;
    }

    if (i == index) {
      tag = AEPReader::ReadFirstTagByName(&tag.bytes, "GCky");
      if (tag.bytes.empty()) {
        break;
      }

      for (int k = 0; tag.bytes.bytesAvailable() && k <= keyframeIndex; ++k) {
        auto stringTag = AEPReader::ReadTag(&tag.bytes);
        if (stringTag.bytes.empty()) {
          break;
        }
        if (k == keyframeIndex) {
          gradientText = stringTag.bytes.readUTF8String();
          break;
        }
      }
      break;
    }
  }

  return ParseGradientColor(gradientText);
}

pag::TextDirection PAGExportSession::currentTextDocumentDirection() {
  if (keyframeNum > 1 && recordCompId == curCompId && recordLayerId == curLayerId &&
      keyframeNum == static_cast<int>(textDirectList.size())) {
    return textDirectList[keyframeIndex];
  }
  recordCompId = 0;
  recordLayerId = 0;
  textDirectList.clear();

  auto fileBytes = getFileBytes();
  ByteArray bytes(reinterpret_cast<uint8_t*>(fileBytes.get()), static_cast<uint32_t>(fileLength));
  bytes = AEPReader::ReadBody(&bytes);
  auto compositions = AEPReader::ReadCompositions(&bytes);
  ByteArray layerBytes = {};
  for (auto& composition : compositions) {
    if (composition.id == static_cast<int>(curCompId)) {
      auto layers = AEPReader::ReadLayers(&composition.bytes);
      if (static_cast<int>(layers.size()) > layerIndex) {
        layerBytes = layers[layerIndex].bytes;
      }
      break;
    }
  }
  if (layerBytes.empty()) {
    return pag::TextDirection::Horizontal;
  }

  auto tag = AEPReader::ReadFirstGroupByMatchNames(&layerBytes, {"ADBE Text Document"});
  if (tag.bytes.empty()) {
    return pag::TextDirection::Horizontal;
  }

  tag = AEPReader::ReadFirstTagByName(&tag.bytes, "btdk");
  if (tag.bytes.empty()) {
    return pag::TextDirection::Horizontal;
  }

  PsdTextAttribute attribute(tag.bytes.length(), tag.bytes.data());
  attribute.getAttribute();
  for (int index = 0; index == 0 || index < keyframeNum; index++) {
    int textDirect = -1;
    std::vector<int> keys = {0, 0, 8, 0, index, 0, 2, 1};
    auto flag = attribute.getIntegerByKeys(textDirect, keys, keys.size());
    if (flag && textDirect == 2) {
      textDirectList.push_back(pag::TextDirection::Vertical);
    } else {
      textDirectList.push_back(pag::TextDirection::Horizontal);
    }
  }
  recordCompId = curCompId;
  recordLayerId = curLayerId;

  return textDirectList[keyframeIndex];
}

std::shared_ptr<char> PAGExportSession::getFileBytes() {
  if (fileBytes != nullptr) {
    return fileBytes;
  }

  AEGP_ProjectH projectHandle;
  suites->ProjSuite6()->AEGP_GetProjectByIndex(0, &projectHandle);

  AEGP_MemHandle pathMemory;
  suites->ProjSuite6()->AEGP_GetProjectPath(projectHandle, &pathMemory);
  char16_t* projectPath = nullptr;
  suites->MemorySuite1()->AEGP_LockMemHandle(pathMemory, reinterpret_cast<void**>(&projectPath));

  std::string filePath = Utf16ToUtf8(projectPath);
  suites->MemorySuite1()->AEGP_FreeMemHandle(pathMemory);

  A_Boolean isDirty = 0;
  suites->ProjSuite6()->AEGP_ProjectIsDirty(projectHandle, &isDirty);
  bool isAEPX = false;

  if (!filePath.empty()) {
    auto extension = filePath.substr(filePath.size() - 5, 5);
    isAEPX = ToLowerCase(extension) == ".aepx";
  }

  FileHelper::TemporaryFileManager tempFileMgr;
  if (isDirty || isAEPX) {
    filePath = GetTempFolderPath() + u8"/.PAGAutoSave.aep";
    tempFileMgr.setFilePath(filePath);
    auto path = Utf8ToUtf16(filePath);
    suites->ProjSuite6()->AEGP_SaveProjectToPath(
        projectHandle, reinterpret_cast<const A_UTF16Char*>(path.c_str()));
  }

  filePath = ConvertStringEncoding(filePath);

  std::ifstream t(filePath, std::ios::binary);
  if (!t.is_open()) {
    return nullptr;
  }

  t.seekg(0, std::ios::end);
  fileLength = t.tellg();
  t.seekg(0, std::ios::beg);

  fileBytes = std::shared_ptr<char>(new char[fileLength], std::default_delete<char[]>());
  t.read(fileBytes.get(), static_cast<std::streamsize>(fileLength));
  t.close();

  return fileBytes;
}

AEGP_LayerH PAGExportSession::getLayerHById(pag::ID id) {
  auto pair = layerHList.find(id);
  if (pair != layerHList.end()) {
    return pair->second;
  }
  return nullptr;
}

AEGP_ItemH PAGExportSession::getCompItemHById(pag::ID id) {
  auto pair = compItemHList.find(id);
  if (pair != compItemHList.end()) {
    return pair->second;
  }
  return nullptr;
}

bool PAGExportSession::isVideoReplaceLayer(AEGP_LayerH layerH) {
  auto id = AEHelper::GetItemIdFromLayer(layerH);
  for (auto pair : imageLayerHList) {
    if (id == AEHelper::GetItemIdFromLayer(pair.first)) {
      return pair.second;
    }
  }
  return false;
}

void PAGExportSession::pushWarning(AlertInfoType type, const std::string& addInfo) {
  alertInfoManager.pushWarning(compItemHList, layerHList, type, curCompId, curLayerId, addInfo);
}

void PAGExportSession::pushWarning(AlertInfoType type, pag::ID compId, pag::ID layerId,
                                   const std::string& addInfo) {
  alertInfoManager.pushWarning(compItemHList, layerHList, type, compId, layerId, addInfo);
}

}  // namespace exporter
