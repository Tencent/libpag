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
#include "Context.h"
#include <codecvt>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <utility>
#include "AEUtils.h"
#include "CommonMethod.h"
#include "ConfigParam.h"
#include "RunScript.h"
#include "StringUtil.h"
#include "src/exports/PAGDataTypes.h"
#include "src/ui/qt/Progress/ProgressWindow.h"
#include "src/utils/ByteArray.h"
#include "src/utils/aep/AEPReader.h"
#include "src/utils/aep/PsdTextParse.h"
#include "src/ui/qt/EnvConfig.h"

namespace pagexporter {
void ScaleAndFpsListDefault(std::vector<std::pair<float, float>>& scaleAndFpsList) {
  scaleAndFpsList.clear();
  scaleAndFpsList.emplace_back(1.00f, 24.0f);
}

Context::Context(std::string outputPath)
    : pluginID(PLUGIN_ID()), suites(SUITES()), outputPath(std::move(outputPath)), alertInfos(this),
      bEarlyExit(false) {
  // 从config文件里获取配置参数
  ConfigParam configParam;
  ConfigParamManager& paramManager = ConfigParamManager::getInstance();
  paramManager.getConfigParam(configParam);
  setParam(&configParam);
  checkParamValid();
}

Context::~Context() {
  delete[] fileBytes;
  delete progressWindow;
}

void Context::setParam(ConfigParam* configParam) {

  bitmapKeyFrameInterval = configParam->bmpComposParam.bitmapKeyFrameInterval;
  bitmapMaxResolution = configParam->bmpComposParam.bitmapMaxResolution;

  tagMode = configParam->commonParam.exportVersion;
  scenes = configParam->commonParam.exportScene;
  exportTagLevel = configParam->commonParam.tagLevel;
  imageQuality = configParam->commonParam.imageQuality;
  imagePixelRatio = configParam->commonParam.imagePixelRatio;

  for (auto& [fst, snd] : configParam->scaleAndFpsList) {
    scaleAndFpsList.emplace_back(fst, snd);
  }
  sequenceType = configParam->sequenceType;
  sequenceSuffix = configParam->sequenceSuffix;
  enableLayerName = configParam->commonParam.enableLayerName;
  enableFontFile = configParam->commonParam.enableFontFile;
  sequenceQuality = configParam->bmpComposParam.sequenceQuality;
  enableCompressionPanel = configParam->enableCompressionPanel;
}

void Context::checkParamValid() {
  if (!scaleAndFpsList.empty()) {
    for (auto& scaleAndFps : scaleAndFpsList) {
      if (scaleAndFps.first >= 1.0) {  // scaleFactor
        scaleAndFps.first = 1.0;
      } else if (scaleAndFps.first < 0.01) {
        scaleAndFps.first = 0.01;
      }
      if (scaleAndFps.second > 120) {  // max frameRate
        scaleAndFps.second = 120;
      } else if (scaleAndFps.second < 0.01) {
        scaleAndFps.second = 0.01;
      }
    }
  } else {  // 如果用户没有设置，就写入默认值
    ScaleAndFpsListDefault(scaleAndFpsList);
  }

  switch (tagMode) {
    case ExportVersionType::Beta:
      exportTagLevel = static_cast<uint16_t>(PresetTagLevel::TagLevelMax);
      break;
    case ExportVersionType::Stable:
      exportTagLevel = static_cast<uint16_t>(PresetTagLevel::TagLevelStable);
      break;
    case ExportVersionType::Custom:
      break;
    default:
      printf("Error! unsupported ExportTagMode(%d)\n", static_cast<int>(tagMode));

      tagMode = ExportVersionType::Stable;
      exportTagLevel = static_cast<uint16_t>(PresetTagLevel::TagLevelStable);
      break;
  }

  if (exportTagLevel < static_cast<uint16_t>(PresetTagLevel::TagLevelMin)) {
    exportTagLevel = static_cast<uint16_t>(PresetTagLevel::TagLevelMin);
  } else if (exportTagLevel > static_cast<uint16_t>(PresetTagLevel::TagLevelMax)) {
    exportTagLevel = static_cast<uint16_t>(PresetTagLevel::TagLevelMax);
  }

  if (imageQuality < 0) {
    imageQuality = 0;
  } else if (imageQuality > 100) {
    imageQuality = 100;
  }

  if (imagePixelRatio < 1.0) {
    imagePixelRatio = 1.0;
  } else if (imagePixelRatio > 3.0) {
    imagePixelRatio = 3.0;
  }

  transform(sequenceSuffix.begin(), sequenceSuffix.end(), sequenceSuffix.begin(), ::tolower);
  if (sequenceSuffix.empty()) {
    sequenceSuffix = "_bmp";
  }
  if (sequenceType != pag::CompositionType::Video && sequenceType != pag::CompositionType::Bitmap) {
    sequenceType = DEFAULT_SEQUENCE_TYPE;
  }

  if (sequenceQuality < 0) {
    sequenceQuality = 0;
  } else if (sequenceQuality > 100) {
    sequenceQuality = 100;
  }
}

inline std::string IDToString(pag::ID id) {
  std::ostringstream result;
  result << id;
  return result.str();
}

inline std::string ToString(int index) {
  std::ostringstream result;
  result << index;
  return result.str();
}

inline bool ToBoolean(const std::string& value, bool defaultValue) {
  if (value.empty()) {
    return defaultValue;
  }
  return (StringUtil::ToLowerCase(value) == "true");
}

inline float ToFloat(const std::string& value, float defaultValue) {
  if (value.empty()) {
    return defaultValue;
  }
  return std::stof(value);
}

inline float ExportLeading(const std::string& value, float fontSize) {
  if (value.empty()) {
    return 0;
  }
  const auto lines = StringUtil::Split(value, ",");
  if (lines.size() < 6) {
    return 0;
  }
  const auto firstY = static_cast<float>(std::stod(lines[1]));
  const auto secondY = static_cast<float>(std::stod(lines[5]));
  const auto leading = roundf(secondY - firstY);
  if (leading == roundf(fontSize * 1.2f)) {
    return 0;
  }
  if (leading > 100000000) {
    return 0;
  }
  return leading;
}

inline float ExportFirstBaseLine(const std::string& value, const float lineHeight,
                                 const float baselineShift, const bool isVertical) {
  if (value.empty()) {
    return 0;
  }
  const auto lines = StringUtil::Split(value, ",");
  if (lines.size() < 4) {
    return 0;
  }

  const auto lineCount = floorf(lines.size() * 1.0f / 4);
  if (lineCount < 1) {
    return 0;
  }
  const auto index = static_cast<int>((lineCount - 1) * 4);
  //The first line or last line can randomly be a infinity value when there is only one char in each line.
  float firstBaseLine;
  if (isVertical) {
    firstBaseLine = static_cast<float>(std::stod(lines[0]));
    if (fabsf(firstBaseLine) > 100000000) {
      auto lastBaseLine = static_cast<float>(std::stod(lines[index + 0]));
      firstBaseLine = lastBaseLine + lineHeight * (lineCount - 1);
    }
  } else {
    firstBaseLine = static_cast<float>(std::stod(lines[1]));
    if (fabsf(firstBaseLine) > 100000000) {
      auto lastBaseLine = static_cast<float>(std::stod(lines[index + 1]));
      firstBaseLine = lastBaseLine - lineHeight * (lineCount - 1);
    }
  }
  if (fabsf(firstBaseLine) > 100000000) {
    firstBaseLine = 0;
  }

  if (isVertical) {
    return firstBaseLine - baselineShift;
  } else {
    return firstBaseLine + baselineShift;
  }
}

inline pag::Color ToColor(const std::string& value, const pag::Color defaultValue) {
  if (value.empty()) {
    return defaultValue;
  }
  const auto lines = StringUtil::Split(value, ",");
  if (lines.size() < 3) {
    return defaultValue;
  }
  pag::Color color = {};
  color.red = static_cast<uint8_t>(std::stod(lines[0]) * 255);
  color.green = static_cast<uint8_t>(std::stod(lines[1]) * 255);
  color.blue = static_cast<uint8_t>(std::stod(lines[2]) * 255);
  return color;
}

inline pag::Point ToPoint(const std::string& value, pag::Point defaultValue) {
  if (value.empty()) {
    return defaultValue;
  }
  const auto lines = StringUtil::Split(value, ",");
  if (lines.size() < 2) {
    return defaultValue;
  }
  pag::Point point = {};
  point.x = static_cast<float>(std::stod(lines[0]));
  point.y = static_cast<float>(std::stod(lines[1]));
  return point;
}

inline std::string FormatString(const std::string& value, const std::string& defaultValue) {
  if (value.empty()) {
    return defaultValue;
  }
  return StringUtil::ReplaceAll(value, "\\n", "\n");
}

inline std::string GetValue(const std::unordered_map<std::string, std::string>& map,
                            const std::string& key) {
  if (const auto result = map.find(key); result != map.end()) {
    return result->second;
  }
  return "";
}

void Context::exportFontFile(const pag::TextDocument* textDocument,
                             const std::unordered_map<std::string, std::string>& valueMap) {
  if (!this->enableFontFile) {
    return;
  }
  auto pathIndex = this->outputPath.find_last_of('/');
  auto path = this->outputPath.substr(0, pathIndex);
  path += "/" + textDocument->fontFamily + "-" + textDocument->fontStyle + ".ttc";
  for (auto& fontFilePath : fontFilePathList) {
    if (path == fontFilePath) {
      return;
    }
  }
  fontFilePathList.push_back(path);
  const auto fontLocation = FormatString(GetValue(valueMap, "fontLocation"), "");
  const auto cmd = "cp \"" + fontLocation + "\" \"" + path + "\"";
  system(cmd.c_str());

  if (const int size = FileIO::GetFileSize(path); size > 30 * 1000 * 1000) {
    pushWarning(pagexporter::AlertInfoType::FontFileTooBig,
                textDocument->fontFamily + ", " + std::to_string(size / 1000 / 1000));
  }
}

pag::TextDocumentHandle Context::currentTextDocument() {
  auto id = IDToString(curCompId);
  auto layer = ToString(layerIndex);
  auto keyframe = ToString(keyframeIndex);
  std::string result;
  if (enableRunScript) {
    AEUtils::RegisterTextDocumentScript();
    auto code = "PAG.printTextDocuments(" + id + ", " + layer + ", " + keyframe + ");";
    result = RunScript(suites, pluginID, code);
  }
  if (result.empty()) {
    return std::make_shared<pag::TextDocument>();
  }
  std::unordered_map<std::string, std::string> valueMap;
  auto lines = StringUtil::Split(result, "\n");
  for (auto& line : lines) {
    auto index = line.find(" : ");
    if (index == std::string::npos) {
      continue;
    }
    auto key = line.substr(0, index);
    auto value = line.substr(index + 3);
    valueMap.insert(std::make_pair(key, value));
  }
  auto textDocument = new pag::TextDocument();
  textDocument->direction = currentTextDocumentDirection();
  textDocument->applyFill = ToBoolean(GetValue(valueMap, "applyFill"), textDocument->applyFill);
  textDocument->applyStroke =
      ToBoolean(GetValue(valueMap, "applyStroke"), textDocument->applyStroke);
  textDocument->baselineShift =
      ToFloat(GetValue(valueMap, "baselineShift"), textDocument->baselineShift);
  textDocument->boxText = ToBoolean(GetValue(valueMap, "boxText"), textDocument->boxText);
  textDocument->boxTextPos = ToPoint(GetValue(valueMap, "boxTextPos"), textDocument->boxTextPos);
  textDocument->boxTextSize = ToPoint(GetValue(valueMap, "boxTextSize"), textDocument->boxTextSize);
  textDocument->fauxBold = ToBoolean(GetValue(valueMap, "fauxBold"), textDocument->fauxBold);
  textDocument->fauxItalic = ToBoolean(GetValue(valueMap, "fauxItalic"), textDocument->fauxItalic);
  textDocument->fillColor = ToColor(GetValue(valueMap, "fillColor"), textDocument->fillColor);
  textDocument->fontFamily = FormatString(GetValue(valueMap, "fontFamily"), "");
  textDocument->fontStyle = FormatString(GetValue(valueMap, "fontStyle"), "");
  textDocument->fontSize = ToFloat(GetValue(valueMap, "fontSize"), textDocument->fontSize);
  auto justification = ToFloat(GetValue(valueMap, "justification"), textDocument->justification);
  textDocument->strokeColor = ToColor(GetValue(valueMap, "strokeColor"), textDocument->strokeColor);
  textDocument->strokeOverFill =
      ToBoolean(GetValue(valueMap, "strokeOverFill"), textDocument->strokeOverFill);
  textDocument->strokeWidth = ToFloat(GetValue(valueMap, "strokeWidth"), textDocument->strokeWidth);
  textDocument->text = FormatString(GetValue(valueMap, "text"), textDocument->text);
  textDocument->justification = ExportParagraphJustification(static_cast<int>(justification));
  textDocument->leading = ToFloat(GetValue(valueMap, "leading"), textDocument->leading);
  if (textDocument->leading == 0) {
    textDocument->leading =
        ExportLeading(GetValue(valueMap, "baselineLocs"), textDocument->fontSize);
  }
  textDocument->tracking = ToFloat(GetValue(valueMap, "tracking"), textDocument->tracking);
  auto lineHeight =
      textDocument->leading == 0 ? roundf(textDocument->fontSize * 1.2f) : textDocument->leading;
  textDocument->firstBaseLine = ExportFirstBaseLine(
      GetValue(valueMap, "baselineLocs"), lineHeight, textDocument->baselineShift,
      textDocument->direction == pag::TextDirection::Vertical);
  exportFontFile(textDocument, valueMap);

  return pag::TextDocumentHandle(textDocument);
}

static std::vector<float> ParseFloats(const std::string& text) {
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

template <typename T>
static void ResortGradientStops(std::vector<T>& list) {
  std::sort(list.begin(), list.end(),
            [](const T& a, const T& b) { return a.position < b.position; });
}

static std::vector<pag::AlphaStop> ParseAlphaStops(const std::string& text) {
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

static std::vector<pag::ColorStop> ParseColorStops(const std::string& text) {
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

static pag::GradientColorHandle DefaultGradientColors() {
  const auto gradientColor = new pag::GradientColor();
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
  return pag::GradientColorHandle(gradientColor);
}

static pag::GradientColorHandle ParseGradientColor(const std::string& text) {
  if (text.empty()) {
    return DefaultGradientColors();
  }
  auto gradientColor = new pag::GradientColor();
  gradientColor->alphaStops = ParseAlphaStops(text);
  gradientColor->colorStops = ParseColorStops(text);
  if (gradientColor->alphaStops.size() < 2 || gradientColor->colorStops.size() < 2) {
    delete gradientColor;
    return DefaultGradientColors();
  }
  ResortGradientStops(gradientColor->alphaStops);
  ResortGradientStops(gradientColor->colorStops);
  return pag::GradientColorHandle(gradientColor);
}

pag::GradientColorHandle Context::currentGradientColors(const std::vector<std::string>& matchNames,
                                                        int index) {
  auto fileBytes = getFileBytes();
  ByteArray bytes(reinterpret_cast<uint8_t*>(fileBytes), static_cast<uint32_t>(fileLength));
  bytes = aep::ReadBody(&bytes);
  auto compositions = aep::ReadCompositions(&bytes);
  ByteArray layerBytes;
  for (auto& composition : compositions) {
    if (composition.id == static_cast<int>(curCompId)) {
      auto layers = aep::ReadLayers(&composition.bytes);
      if (static_cast<int>(layers.size()) > layerIndex) {
        layerBytes = layers[layerIndex].bytes;
      }
      break;
    }
  }
  if (layerBytes.empty()) {
    return DefaultGradientColors();
  }
  int i = 0;
  std::string gradientText;
  while (layerBytes.bytesAvailable() > 0) {
    auto tag = aep::ReadFirstGroupByMatchNames(&layerBytes, matchNames);
    if (tag.bytes.empty()) {
      break;
    }
    if (i == index) {
      tag = aep::ReadFirstTagByName(&tag.bytes, "GCky");
      if (tag.bytes.empty()) {
        break;
      }
      int k = 0;
      while (tag.bytes.bytesAvailable()) {
        auto [name, bytes] = aep::ReadTag(&tag.bytes);
        if (bytes.empty()) {
          break;
        }
        if (k == keyframeIndex) {
          gradientText = bytes.readUTF8String();
          break;
        }
        k++;
      }
      break;
    }
    i++;
  }
  return ParseGradientColor(gradientText);
}

pag::Enum Context::currentTextDocumentDirection() {
  if (keyframeNum > 1 && recordCompId == curCompId && recordLayerId == curLayerId &&
      keyframeNum == static_cast<int>(textDirectList.size())) {
    return textDirectList[keyframeIndex];
  }
  recordCompId = 0;
  recordLayerId = 0;
  textDirectList.clear();

  const auto fileBytes = getFileBytes();
  ByteArray bytes(reinterpret_cast<uint8_t*>(fileBytes), static_cast<uint32_t>(fileLength));
  bytes = aep::ReadBody(&bytes);
  auto compositions = aep::ReadCompositions(&bytes);
  ByteArray layerBytes;
  for (auto& composition : compositions) {
    if (composition.id == static_cast<int>(curCompId)) {
      if (const auto layers = aep::ReadLayers(&composition.bytes);
          static_cast<int>(layers.size()) > layerIndex) {
        layerBytes = layers[layerIndex].bytes;
      }
      break;
    }
  }
  if (layerBytes.empty()) {
    return pag::TextDirection::Horizontal;
  }

  auto tag = aep::ReadFirstGroupByMatchNames(&layerBytes, {"ADBE Text Document"});
  if (tag.bytes.empty()) {
    return pag::TextDirection::Horizontal;
  }

  tag = aep::ReadFirstTagByName(&tag.bytes, "btdk");
  if (tag.bytes.empty()) {
    return pag::TextDirection::Horizontal;
  }

  aep::PsdTextAttribute attribute(tag.bytes.length(), tag.bytes.data());
  attribute.getAttibute();
  for (int index = 0; index == 0 || index < keyframeNum; index++) {
    int textDirect = -1;
    int keys[] = {0, 0, 8, 0, index, 0, 2, 1};
    if (const auto flag = attribute.getIntegerByKeys(textDirect, keys, std::size(keys));
        flag && textDirect == 2) {
      textDirectList.push_back(static_cast<pag::Enum>(pag::TextDirection::Vertical));
    } else {
      textDirectList.push_back(static_cast<pag::Enum>(pag::TextDirection::Horizontal));
    }
  }
  recordCompId = curCompId;
  recordLayerId = curLayerId;
  return textDirectList[keyframeIndex];
  return 0;
}

char* Context::getFileBytes() {
  if (fileBytes != nullptr) {
    return fileBytes;
  }
  AEGP_ProjectH projectHandle;
  suites.ProjSuite6()->AEGP_GetProjectByIndex(0, &projectHandle);
  AEGP_MemHandle pathMemory;
  suites.ProjSuite6()->AEGP_GetProjectPath(projectHandle, &pathMemory);
  char16_t* projectPath = nullptr;
  suites.MemorySuite1()->AEGP_LockMemHandle(pathMemory, reinterpret_cast<void**>(&projectPath));
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
  std::string filePath = convert.to_bytes(projectPath);
  suites.MemorySuite1()->AEGP_FreeMemHandle(pathMemory);

  A_Boolean isDirty = 0;
  suites.ProjSuite6()->AEGP_ProjectIsDirty(projectHandle, &isDirty);
  bool isAEPX = false;
  if (!filePath.empty()) {
    auto extension = filePath.substr(filePath.size() - 5, 5);
    isAEPX = StringUtil::ToLowerCase(extension) == ".aepx";
  }
  if (isDirty || isAEPX) {
    filePath = AEUtils::GetFolderTempName() + u8"/.PAGAutoSave.aep";
    auto path = convert.from_bytes(filePath);
    suites.ProjSuite6()->AEGP_SaveProjectToPath(projectHandle,
                                                reinterpret_cast<const A_UTF16Char*>(path.c_str()));
  }
  filePath = QStringToString(filePath);
  TemporaryFileManager tempFileMgr;
  tempFileMgr.tempFilePath = filePath;

  std::ifstream t;
  t.open(filePath, std::ios::binary);
  t.seekg(0, std::ios::end);
  auto fileSize = t.tellg();
  fileLength = static_cast<size_t>(fileSize);
  t.seekg(0, std::ios::beg);
  fileBytes = new char[fileLength];
  t.read(fileBytes, fileLength);
  t.close();
  return fileBytes;
}

AEGP_LayerH Context::getLayerHById(pag::ID id) {
  auto pair = layerHList.find(id);
  if (pair != layerHList.end()) {
    return pair->second;
  }
  return nullptr;
}

AEGP_ItemH Context::getCompItemHById(pag::ID id) {
  auto pair = compItemHList.find(id);
  if (pair != compItemHList.end()) {
    return pair->second;
  }
  return nullptr;
}

bool Context::isVideoReplaceLayer(const AEGP_LayerH layerH) const {
  const auto id = AEUtils::GetItemIdFromLayer(layerH);
  for (auto [fst, snd] : imageLayerHList) {
    if (id == AEUtils::GetItemIdFromLayer(fst)) {
      return snd;
    }
  }
  return false;
}

void Context::pushWarning(const pagexporter::AlertInfoType type, std::string addInfo) {
  alertInfos.pushWarning(type, std::move(addInfo));
}

void Context::pushWarning(const pagexporter::AlertInfoType type, const pag::ID compId,
                          const pag::ID layerId, const std::string& addInfo) {
  alertInfos.pushWarning(type, compId, layerId, addInfo);
}
}  // namespace pagexporter
