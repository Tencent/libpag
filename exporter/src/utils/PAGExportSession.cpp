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
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include "AEHelper.h"
#include "AEPReader.h"
#include "ByteArray.h"
#include "FileHelper.h"
#include "PsdTextAttribute.h"
#include "StringHelper.h"
#include "nlohmann/json.hpp"
#include "platform/PlatformHelper.h"
#include "src/base/utils/Log.h"
#include "tinyxml2.h"
#include "utils/TempFileDelete.h"

namespace fs = std::filesystem;
using namespace StringHelper;
using namespace tinyxml2;
using json = nlohmann::json;

namespace exporter {

PAGExportSession::PAGExportSession(const std::string& path)
    : pluginID(AEHelper::GetPluginID()), suites(AEHelper::GetSuites()), outputPath(path),
      bEarlyExit(false) {
  checkParamValid();
}

PAGExportSession::~PAGExportSession() {
}

void PAGExportSession::checkParamValid() {
  const ConfigParam tempParam;

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

  configParam.sequenceSuffix = ToLowerCase(configParam.sequenceSuffix);
  if (configParam.sequenceSuffix.empty()) {
    configParam.sequenceSuffix = tempParam.sequenceSuffix;
  }
  if (configParam.sequenceType != pag::CompositionType::Video &&
      configParam.sequenceType != pag::CompositionType::Bitmap) {
    configParam.sequenceType = tempParam.sequenceType;
  }

  configParam.sequenceQuality = std::clamp(configParam.sequenceQuality, 0, 100);
}

pag::TextDocumentHandle PAGExportSession::currentTextDocument() {
  auto id = std::to_string(curCompId);
  auto layer = std::to_string(layerIndex);
  auto keyframe = std::to_string(keyframeIndex);
  std::string result = "";
  if (enableRunScript) {
    AEHelper::RegisterTextDocumentScript();
    std::stringstream ss;
    ss << "PAG.printTextDocuments(" << id << ", " << layer << ", " << keyframe << ");";
    auto code = ss.str();
    result = AEHelper::RunScript(suites, pluginID, code);
  }
  if (result.empty()) {
    return std::make_shared<pag::TextDocument>();
  }

  json jsonData = json::parse(result);

  auto textDocument = std::make_shared<pag::TextDocument>();
  textDocument->direction = currentTextDocumentDirection();
  textDocument->applyFill = jsonData.value("applyFill", textDocument->applyFill);
  textDocument->applyStroke = jsonData.value("applyStroke", textDocument->applyStroke);
  textDocument->baselineShift = jsonData.value("applyStroke", textDocument->applyStroke);
  textDocument->boxText = jsonData.value("boxText", textDocument->boxText);
  textDocument->boxTextPos = StringToPoint(jsonData.value("boxTextPos", std::vector<std::string>()),
                                           textDocument->boxTextPos);
  textDocument->boxTextSize = StringToPoint(
      jsonData.value("boxTextSize", std::vector<std::string>()), textDocument->boxTextSize);
  textDocument->fauxBold = jsonData.value("fauxBold", textDocument->fauxBold);
  textDocument->fauxItalic = jsonData.value("fauxItalic", textDocument->fauxItalic);
  textDocument->fillColor = StringToColor(jsonData.value("fillColor", std::vector<std::string>()),
                                          textDocument->fillColor);
  textDocument->fontFamily = jsonData.value("fontFamily", textDocument->fontFamily);
  textDocument->fontStyle = jsonData.value("fontStyle", textDocument->fontStyle);
  textDocument->fontSize = jsonData.value("fontSize", textDocument->fontSize);
  textDocument->strokeColor = StringToColor(
      jsonData.value("strokeColor", std::vector<std::string>()), textDocument->strokeColor);
  textDocument->strokeOverFill = jsonData.value("strokeOverFill", textDocument->strokeOverFill);
  textDocument->strokeWidth = jsonData.value("strokeWidth", textDocument->strokeWidth);
  textDocument->text = jsonData.value("text", textDocument->text);
  textDocument->justification =
      IntToParagraphJustification(jsonData.value("justification", -1), textDocument->justification);
  textDocument->leading = jsonData.value("leading", textDocument->leading);
  if (textDocument->leading == 0) {
    textDocument->leading = CalculateLineSpacing(
        jsonData.value("baselineLocs", std::vector<std::string>()), textDocument->fontSize);
  }
  textDocument->tracking = jsonData.value("tracking", textDocument->tracking);
  auto lineHeight =
      textDocument->leading == 0 ? roundf(textDocument->fontSize * 1.2f) : textDocument->leading;
  textDocument->firstBaseLine = CalculateFirstBaseline(
      jsonData.value("baselineLocs", std::vector<std::string>()), lineHeight,
      textDocument->baselineShift, textDocument->direction == pag::TextDirection::Vertical);

  std::stringstream ss;
  auto pathIndex = this->outputPath.find_last_of('/');
  ss << this->outputPath.substr(0, pathIndex) << "/" << textDocument->fontFamily << "-"
     << textDocument->fontStyle << ".ttc";
  auto path = ss.str();
  fontFilePathList.emplace_back(path);
  auto fontLocation = jsonData.value("fontLocation", "");

  FileHelper::CopyFile(fontLocation, path);
  size_t size = FileHelper::GetFileSize(path);
  if (size > 30 * 1024 * 1024) {  // 30MB
    pushWarning(AlertInfoType::FontFileTooBig,
                textDocument->fontFamily + ", " + std::to_string(size / 1024 / 1024) + "MB");
  }

  return textDocument;
}

std::vector<std::vector<float>> PAGExportSession::extractFloatArraysByKey(
    const std::string& xmlContent, const std::string& keyName) {
  std::vector<std::vector<float>> result = {};
  XMLDocument doc;
  if (doc.Parse(xmlContent.c_str()) != XML_SUCCESS) {
    LOGE("XML parsing failed: %s", doc.ErrorStr());

    return result;
  }

  auto traverse = [&](XMLElement* element, const auto& traverseRef) {
    if (!element) return;

    for (XMLElement* child = element->FirstChildElement(); child != nullptr;
         child = child->NextSiblingElement()) {
      if (std::string(child->Value()) == "prop.pair") {
        XMLElement* key = child->FirstChildElement("key");
        if (key && key->GetText()) {
          if (std::string(key->GetText()) == keyName) {
            XMLElement* array = child->FirstChildElement("array");
            if (array) {
              XMLElement* arrayType = array->FirstChildElement("array.type");
              if (arrayType && arrayType->FirstChildElement("float")) {
                std::vector<float> floatList = {};
                for (XMLElement* floatVal = array->FirstChildElement("float"); floatVal != nullptr;
                     floatVal = floatVal->NextSiblingElement("float")) {
                  if (floatVal->GetText()) {
                    try {
                      floatList.emplace_back(std::stof(floatVal->GetText()));
                    } catch (const std::exception& e) {
                      LOGE("Error converting float value: %s", e.what());
                    }
                  }
                }
                result.emplace_back(floatList);
              }
            }
          }
        }
      }
      traverseRef(child, traverseRef);
    }
  };

  traverse(doc.RootElement(), traverse);

  return result;
}

std::vector<pag::AlphaStop> PAGExportSession::ParseAlphaStops(const std::string& text) {
  std::vector<pag::AlphaStop> list = {};

  auto alphaStopList = extractFloatArraysByKey(text, "Stops Alpha");
  for (const auto& alphaStop : alphaStopList) {
    if (alphaStop.size() < 3) {
      return {};
    }
    pag::AlphaStop stop = {};
    stop.position = alphaStop[0];
    stop.midpoint = alphaStop[1];
    stop.opacity = static_cast<uint8_t>(alphaStop[2] * 255);
    list.emplace_back(stop);
  }
  return list;
}

std::vector<pag::ColorStop> PAGExportSession::ParseColorStops(const std::string& text) {
  std::vector<pag::ColorStop> list = {};
  auto colorStopList = extractFloatArraysByKey(text, "Stops Color");
  for (const auto& colorStop : colorStopList) {
    if (colorStop.size() < 6) {
      return {};
    }
    pag::Color color = {};
    color.red = static_cast<uint8_t>(255 * colorStop[2]);
    color.green = static_cast<uint8_t>(255 * colorStop[3]);
    color.blue = static_cast<uint8_t>(255 * colorStop[4]);
    pag::ColorStop stop = {};
    stop.position = colorStop[0];
    stop.midpoint = colorStop[1];
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
  gradientColor->alphaStops.emplace_back(stop);
  stop = {};
  stop.position = 1.0f;
  gradientColor->alphaStops.emplace_back(stop);
  pag::ColorStop colorStop = {};
  colorStop.position = 0.0f;
  colorStop.color = pag::White;
  gradientColor->colorStops.emplace_back(colorStop);
  colorStop = {};
  colorStop.position = 1.0f;
  colorStop.color = pag::Black;
  gradientColor->colorStops.emplace_back(colorStop);
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
  if (fileBytes.empty()) {
    return DefaultGradientColors();
  }

  ByteArray bytes(reinterpret_cast<uint8_t*>(fileBytes.data()), fileBytes.size());
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
  ByteArray bytes(reinterpret_cast<uint8_t*>(fileBytes.data()), fileBytes.size());
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
      textDirectList.emplace_back(pag::TextDirection::Vertical);
    } else {
      textDirectList.emplace_back(pag::TextDirection::Horizontal);
    }
  }
  recordCompId = curCompId;
  recordLayerId = curLayerId;

  return textDirectList[keyframeIndex];
}

const std::vector<char>& PAGExportSession::getFileBytes() {
  if (!fileBytes.empty()) {
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

  TempFileDelete tempFile;
  if (isDirty || isAEPX) {
    filePath = GetTempFolderPath() + u8"/.PAGAutoSave.aep";
    tempFile.setFilePath(filePath);
    auto path = Utf8ToUtf16(filePath);
    suites->ProjSuite6()->AEGP_SaveProjectToPath(
        projectHandle, reinterpret_cast<const A_UTF16Char*>(path.c_str()));
  }

  filePath = ConvertStringEncoding(filePath);

  std::ifstream t(filePath, std::ios::binary);
  if (!t.is_open()) {
    return fileBytes;
  }

  t.seekg(0, std::ios::end);
  auto fileLength = t.tellg();
  t.seekg(0, std::ios::beg);

  fileBytes.resize(fileLength);
  t.read(fileBytes.data(), fileLength);
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
  for (const auto& pair : imageLayerHList) {
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
