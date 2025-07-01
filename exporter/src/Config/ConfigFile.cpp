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

#include "ConfigFile.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include "ConfigParam.h"
#include "platform/PlatformHelper.h"
#include "tinyxml.h"
#include "utils/ConfigUtils.h"

namespace exporter {
static void ReadCommonConfig(TiXmlElement* commonElement, ConfigParam* configParam) {
  if (!commonElement) {
    return;
  }

  if (TiXmlElement* tagLevelElement = commonElement->FirstChildElement("tag-level")) {
    if (const auto* tagModeText = GetChildElementText(tagLevelElement, "mode")) {
      if (safeStringEqual(tagModeText, "beta")) {
        configParam->tagMode = TagMode::Beta;
      } else if (safeStringEqual(tagModeText, "custom")) {
        configParam->tagMode = TagMode::Custom;
      } else {
        configParam->tagMode = TagMode::Stable;
      }
    }

    if (const auto* customLevelText = GetChildElementText(tagLevelElement, "custom-level")) {
      configParam->exportTagLevel = safeStringToInt<uint16_t>(customLevelText, 1023);
    }
  }

  if (const auto* imageQualityText = GetChildElementText(commonElement, "image-quality")) {
    configParam->imageQuality = safeStringToInt(imageQualityText, 80);
  }

  if (const auto* imagePixelRatioText = GetChildElementText(commonElement, "image-pixel-ratio")) {
    configParam->imagePixelRatio = safeStringToFloat(imagePixelRatioText, 2.0f);
  }

  if (const auto* enableLayerNameText = GetChildElementText(commonElement, "enable-layer-name")) {
    configParam->enableLayerName = safeStringToInt(enableLayerNameText, 1) != 0;
  }

  if (const auto* enableCompressionPanelText =
          GetChildElementText(commonElement, "enable-compression-panel")) {
    configParam->enableCompressionPanel = safeStringToInt(enableCompressionPanelText, 0) != 0;
  }

  if (const auto* enableFontFileText = GetChildElementText(commonElement, "enable-font-file")) {
    configParam->enableFontFile = safeStringToInt(enableFontFileText, 0) != 0;
  }

  if (const auto* exportScenseText = GetChildElementText(commonElement, "export-scense")) {
    configParam->scenes = static_cast<ExportScenes>(safeStringToInt(exportScenseText, 0));
  }

  if (const auto* exportLanguageText = GetChildElementText(commonElement, "export-language")) {
    configParam->language = static_cast<Language>(safeStringToInt(exportLanguageText, 0));
  }
}

static void ReadBitmapConfig(TiXmlElement* bitmapElement, ConfigParam* configParam) {
  if (!bitmapElement) {
    return;
  }

  if (const auto* sequenceSuffixText = GetChildElementText(bitmapElement, "sequence-suffix")) {
    configParam->sequenceSuffix = sequenceSuffixText;
  }

  if (const auto* sequenceTypeText = GetChildElementText(bitmapElement, "sequence-type")) {
    configParam->sequenceType =
        static_cast<pag::CompositionType>(safeStringToInt(sequenceTypeText, 1));
  }

  if (const auto* sequenceQualityText = GetChildElementText(bitmapElement, "sequence-quality")) {
    configParam->sequenceQuality = safeStringToInt(sequenceQualityText, 80);
  }

  if (const auto* keyframeIntervalText = GetChildElementText(bitmapElement, "keyframe-interval")) {
    configParam->bitmapKeyFrameInterval = safeStringToInt(keyframeIntervalText, 60);
  }

  if (const auto* maxResolutionText = GetChildElementText(bitmapElement, "max-resolution")) {
    configParam->bitmapMaxResolution = safeStringToInt(maxResolutionText, 720);
  }

  if (TiXmlElement* sequencesElement = bitmapElement->FirstChildElement("sequences")) {
    if (const TiXmlElement* sequenceElement = sequencesElement->FirstChildElement("sequence")) {
      double frameRate = 24.0;
      if (sequenceElement->Attribute("framerate", &frameRate)) {
        configParam->frameRate = static_cast<float>(frameRate);
      }
    }
  }

  if (configParam->frameRate <= 0) {
    configParam->frameRate = 24.0f;
  }
}

static void WriteCommonConfig(TiXmlElement* root, ConfigParam* configParam) {
  auto common = std::make_unique<TiXmlElement>("common");
  auto commonPtr = common.get();
  root->LinkEndChild(common.release());

  auto tagLevel = std::make_unique<TiXmlElement>("tag-level");
  auto tagLevelPtr = tagLevel.get();
  commonPtr->LinkEndChild(tagLevel.release());

  auto mode = std::make_unique<TiXmlElement>("mode");
  auto modePtr = mode.get();
  switch (configParam->tagMode) {
    case TagMode::Beta:
      modePtr->LinkEndChild(std::make_unique<TiXmlText>("beta").release());
      break;
    case TagMode::Custom:
      modePtr->LinkEndChild(std::make_unique<TiXmlText>("custom").release());
      break;
    default:
      modePtr->LinkEndChild(std::make_unique<TiXmlText>("stable").release());
      break;
  }
  tagLevelPtr->LinkEndChild(mode.release());

  auto customLevel = std::make_unique<TiXmlElement>("custom-level");
  auto customLevelPtr = customLevel.get();
  customLevelPtr->LinkEndChild(
      std::make_unique<TiXmlText>(std::to_string(configParam->exportTagLevel).c_str()).release());
  tagLevelPtr->LinkEndChild(customLevel.release());

  AddElement(commonPtr, "image-quality", std::to_string(configParam->imageQuality));
  AddElement(commonPtr, "image-pixel-ratio", FormatFloat(configParam->imagePixelRatio, 1));
  AddElement(commonPtr, "enable-compression-panel",
             std::to_string(configParam->enableCompressionPanel ? 1 : 0));
  AddElement(commonPtr, "enable-layer-name", std::to_string(configParam->enableLayerName ? 1 : 0));
  AddElement(commonPtr, "enable-font-file", std::to_string(configParam->enableFontFile ? 1 : 0));
  AddElement(commonPtr, "export-scense", std::to_string(static_cast<int>(configParam->scenes)));
  AddElement(commonPtr, "export-language", std::to_string(static_cast<int>(configParam->language)));
}

static void WriteBitmapConfig(TiXmlElement* root, ConfigParam* configParam) {
  auto bitmap = std::make_unique<TiXmlElement>("bitmap");
  auto bitmapPtr = bitmap.get();
  root->LinkEndChild(bitmap.release());

  AddElement(bitmapPtr, "sequence-suffix", configParam->sequenceSuffix);
  AddElement(bitmapPtr, "sequence-type",
             std::to_string(static_cast<int>(configParam->sequenceType)));
  AddElement(bitmapPtr, "sequence-quality", std::to_string(configParam->sequenceQuality));
  AddElement(bitmapPtr, "keyframe-interval", std::to_string(configParam->bitmapKeyFrameInterval));
  AddElement(bitmapPtr, "max-resolution", std::to_string(configParam->bitmapMaxResolution));

  auto sequences = std::make_unique<TiXmlElement>("sequences");
  auto sequencesPtr = sequences.get();
  bitmapPtr->LinkEndChild(sequences.release());

  auto sequence = std::make_unique<TiXmlElement>("sequence");
  auto sequencePtr = sequence.get();
  sequencePtr->SetAttribute("scale", "1");
  sequencePtr->SetAttribute("framerate", FormatFloat(configParam->frameRate, 1).c_str());
  sequencesPtr->LinkEndChild(sequence.release());

  auto vector = std::make_unique<TiXmlElement>("vector");
  auto vectorPtr = vector.get();
  auto vectorText = std::make_unique<TiXmlText>("");
  vectorPtr->LinkEndChild(vectorText.release());
  root->LinkEndChild(vector.release());

  auto video = std::make_unique<TiXmlElement>("video");
  auto videoPtr = video.get();
  auto videoText = std::make_unique<TiXmlText>("");
  videoPtr->LinkEndChild(videoText.release());
  root->LinkEndChild(video.release());
}

int WriteDefaultConfigFile(std::string_view fileName) {
  if (fileName.empty()) {
    return -1;
  }

  TiXmlDocument doc;
  auto decl = std::make_unique<TiXmlDeclaration>("1.0", "utf-8", "");
  doc.LinkEndChild(decl.release());

  auto root = std::make_unique<TiXmlElement>("pag-exporter");
  auto rootPtr = root.get();
  doc.LinkEndChild(root.release());

  ConfigParam defaultConfig;
  WriteCommonConfig(rootPtr, &defaultConfig);
  WriteBitmapConfig(rootPtr, &defaultConfig);

  if (!doc.SaveFile(std::string(fileName).c_str())) {
    return -1;
  }
  return 0;
}

bool ReadConfigFile(ConfigParam* configParam) {
  if (!configParam) {
    return false;
  }

  const std::string configPath = GetConfigPath();
  if (configPath.empty()) {
    return false;
  }

  std::string filename = configPath + "PAGConfig.xml";
  if (!std::filesystem::exists(filename)) {
    WriteDefaultConfigFile(filename.c_str());
    return false;
  }

  TiXmlDocument doc(filename.c_str());
  bool loadOkay = doc.LoadFile();

  if (!loadOkay) {
    return false;
  }

  TiXmlNode* rootNode = doc.FirstChild("pag-exporter");
  if (!rootNode) {
    return false;
  }

  TiXmlElement* rootElement = rootNode->ToElement();
  if (!rootElement) {
    return false;
  }

  TiXmlElement* commonElement = rootElement->FirstChildElement("common");
  TiXmlElement* bitmapElement = rootElement->FirstChildElement("bitmap");

  if (!commonElement || !bitmapElement) {
    return false;
  }

  ReadCommonConfig(commonElement, configParam);
  ReadBitmapConfig(bitmapElement, configParam);

  return true;
}

void WriteConfigFile(ConfigParam* configParam) {
  if (!configParam) return;

  std::string configPath = GetConfigPath();
  if (configPath.empty()) {
    return;
  }

  std::string filename = configPath + "PAGConfig.xml";

  TiXmlDocument doc;
  auto decl = std::make_unique<TiXmlDeclaration>("1.0", "utf-8", "");
  doc.LinkEndChild(decl.release());

  auto root = std::make_unique<TiXmlElement>("pag-exporter");
  auto rootPtr = root.get();
  doc.LinkEndChild(root.release());

  WriteCommonConfig(rootPtr, configParam);
  WriteBitmapConfig(rootPtr, configParam);

  doc.SaveFile(filename.c_str());
}
}  // namespace exporter
