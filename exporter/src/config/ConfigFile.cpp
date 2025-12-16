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

#include "ConfigFile.h"
#include <filesystem>
#include <fstream>
#include <string>
#include "ConfigParam.h"
#include "platform/PlatformHelper.h"
#include "tinyxml2.h"
#include "utils/ConfigUtils.h"
#include "utils/FileHelper.h"
#include "utils/StringHelper.h"

using namespace tinyxml2;
namespace exporter {
static void ReadCommonConfig(XMLElement* commonElement, ConfigParam* configParam) {
  if (!commonElement) {
    return;
  }

  if (XMLElement* tagLevelElement = commonElement->FirstChildElement("tag-level")) {
    if (const auto tagModeText = GetChildElementText(tagLevelElement, "mode")) {
      if (SafeStringEqual(tagModeText, "beta")) {
        configParam->tagMode = TagMode::Beta;
      } else if (SafeStringEqual(tagModeText, "custom")) {
        configParam->tagMode = TagMode::Custom;
      } else {
        configParam->tagMode = TagMode::Stable;
      }
    }

    if (const auto customLevelText = GetChildElementText(tagLevelElement, "custom-level")) {
      configParam->exportTagLevel = SafeStringToInt<uint16_t>(customLevelText, 1023);
    }
  }

  if (const auto imageQualityText = GetChildElementText(commonElement, "image-quality")) {
    configParam->imageQuality = SafeStringToInt(imageQualityText, 80);
  }

  if (const auto imagePixelRatioText = GetChildElementText(commonElement, "image-pixel-ratio")) {
    configParam->imagePixelRatio = SafeStringToFloat(imagePixelRatioText, 2.0f);
  }

  if (const auto enableLayerNameText = GetChildElementText(commonElement, "enable-layer-name")) {
    configParam->exportLayerName = SafeStringToInt(enableLayerNameText, 1) != 0;
  }

  if (const auto enableFontFileText = GetChildElementText(commonElement, "enable-font-file")) {
    configParam->exportFontFile = SafeStringToInt(enableFontFileText, 0) != 0;
  }

  if (const auto exportScenseText = GetChildElementText(commonElement, "export-scense")) {
    configParam->scenes = static_cast<ExportScenes>(SafeStringToInt(exportScenseText, 0));
  }

  if (const auto exportLanguageText = GetChildElementText(commonElement, "export-language")) {
    configParam->language = static_cast<Language>(SafeStringToInt(exportLanguageText, 0));
  }
}

static void ReadBitmapConfig(XMLElement* bitmapElement, ConfigParam* configParam) {
  if (!bitmapElement) {
    return;
  }

  if (const auto sequenceTypeText = GetChildElementText(bitmapElement, "sequence-type")) {
    configParam->sequenceType =
        static_cast<pag::CompositionType>(SafeStringToInt(sequenceTypeText, 1));
  }

  if (const auto sequenceQualityText = GetChildElementText(bitmapElement, "sequence-quality")) {
    configParam->sequenceQuality = SafeStringToInt(sequenceQualityText, 80);
  }

  if (const auto keyframeIntervalText = GetChildElementText(bitmapElement, "keyframe-interval")) {
    configParam->bitmapKeyFrameInterval = SafeStringToInt(keyframeIntervalText, 60);
  }

  if (const auto maxResolutionText = GetChildElementText(bitmapElement, "max-resolution")) {
    configParam->bitmapMaxResolution = SafeStringToInt(maxResolutionText, 720);
  }

  if (XMLElement* sequencesElement = bitmapElement->FirstChildElement("sequences")) {
    if (const XMLElement* sequenceElement = sequencesElement->FirstChildElement("sequence")) {
      configParam->frameRate = sequenceElement->FloatAttribute("framerate", 24.0);
    }
  }

  if (configParam->frameRate <= 0) {
    configParam->frameRate = 24.0f;
  }
}

static void WriteCommonConfig(XMLElement* root, ConfigParam* configParam) {
  XMLDocument* doc = root->GetDocument();
  XMLElement* common = doc->NewElement("common");
  root->InsertEndChild(common);

  XMLElement* tagLevel = doc->NewElement("tag-level");
  common->InsertEndChild(tagLevel);

  XMLElement* mode = doc->NewElement("mode");
  switch (configParam->tagMode) {
    case TagMode::Beta:
      mode->SetText("beta");
      break;
    case TagMode::Custom:
      mode->SetText("custom");
      break;
    default:
      mode->SetText("stable");
      break;
  }
  tagLevel->InsertEndChild(mode);

  XMLElement* customLevel = doc->NewElement("custom-level");
  customLevel->SetText(std::to_string(configParam->exportTagLevel).c_str());
  tagLevel->InsertEndChild(customLevel);

  AddElement(common, "image-quality", std::to_string(configParam->imageQuality));
  AddElement(common, "image-pixel-ratio", FormatFloat(configParam->imagePixelRatio, 1));
  AddElement(common, "enable-compression-panel", std::to_string(1));
  AddElement(common, "enable-layer-name", std::to_string(configParam->exportLayerName ? 1 : 0));
  AddElement(common, "enable-font-file", std::to_string(configParam->exportFontFile ? 1 : 0));
  AddElement(common, "export-scense", std::to_string(static_cast<int>(configParam->scenes)));
  AddElement(common, "export-language", std::to_string(static_cast<int>(configParam->language)));
}

static void WriteBitmapConfig(XMLElement* root, ConfigParam* configParam) {
  XMLDocument* doc = root->GetDocument();
  XMLElement* bitmap = doc->NewElement("bitmap");
  root->InsertEndChild(bitmap);

  AddElement(bitmap, "sequence-suffix", CompositionBmpSuffix);
  AddElement(bitmap, "sequence-type", std::to_string(static_cast<int>(configParam->sequenceType)));
  AddElement(bitmap, "sequence-quality", std::to_string(configParam->sequenceQuality));
  AddElement(bitmap, "keyframe-interval", std::to_string(configParam->bitmapKeyFrameInterval));
  AddElement(bitmap, "max-resolution", std::to_string(configParam->bitmapMaxResolution));

  XMLElement* sequences = doc->NewElement("sequences");
  bitmap->InsertEndChild(sequences);

  XMLElement* sequence = doc->NewElement("sequence");
  sequence->SetAttribute("scale", "1");
  sequence->SetAttribute("framerate", FormatFloat(configParam->frameRate, 1).c_str());
  sequences->InsertEndChild(sequence);

  XMLElement* vector = doc->NewElement("vector");
  vector->SetText("");
  root->InsertEndChild(vector);

  XMLElement* video = doc->NewElement("video");
  video->SetText("");
  root->InsertEndChild(video);
}

int WriteDefaultConfigFile(const std::string& fileName) {
  if (fileName.empty()) {
    return -1;
  }

  XMLDocument doc;
  XMLDeclaration* decl = doc.NewDeclaration("xml version=\"1.0\" encoding=\"utf-8\"");
  doc.InsertFirstChild(decl);

  XMLElement* root = doc.NewElement("pag-exporter");
  doc.InsertEndChild(root);

  ConfigParam defaultConfig;
  WriteCommonConfig(root, &defaultConfig);
  WriteBitmapConfig(root, &defaultConfig);

  if (doc.SaveFile(std::string(fileName).c_str()) != XML_SUCCESS) {
    return -1;
  }
  return 0;
}

bool ReadConfigFile(ConfigParam* configParam) {
  if (!configParam) {
    return false;
  }

  const auto& configPath = GetConfigPath();
  if (configPath.empty()) {
    return false;
  }

  std::string filename = configPath + "PAGConfig.xml";
  if (!FileIsExist(filename)) {
    WriteDefaultConfigFile(filename.c_str());
    return false;
  }

  XMLDocument doc;
  if (doc.LoadFile(filename.c_str()) != XML_SUCCESS) {
    return false;
  }

  XMLElement* rootElement = doc.FirstChildElement("pag-exporter");
  if (!rootElement) {
    return false;
  }

  XMLElement* commonElement = rootElement->FirstChildElement("common");
  XMLElement* bitmapElement = rootElement->FirstChildElement("bitmap");

  if (!commonElement || !bitmapElement) {
    return false;
  }

  ReadCommonConfig(commonElement, configParam);
  ReadBitmapConfig(bitmapElement, configParam);

  return true;
}

void WriteConfigFile(ConfigParam* configParam) {
  if (!configParam) {
    return;
  }

  const auto& configPath = GetConfigPath();
  if (configPath.empty()) {
    return;
  }

  std::string filename = configPath + "PAGConfig.xml";

  XMLDocument doc;
  XMLDeclaration* decl = doc.NewDeclaration("xml version=\"1.0\" encoding=\"utf-8\"");
  doc.InsertFirstChild(decl);

  XMLElement* root = doc.NewElement("pag-exporter");
  doc.InsertEndChild(root);

  WriteCommonConfig(root, configParam);
  WriteBitmapConfig(root, configParam);

  doc.SaveFile(filename.c_str());
}
}  // namespace exporter
