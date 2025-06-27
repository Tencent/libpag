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
#include <charconv>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include "ConfigParam.h"
#include "platform/PlatformHelper.h"
#include "vendor/TinyXML/tinyxml.h"

namespace exporter {

template <typename T>
T safeStringToInt(const char* str, T defaultValue) {
  if (str == nullptr) {
    return defaultValue;
  }
  std::string_view sv(str);
  if (sv.empty()) {
    return defaultValue;
  }

  T value{};
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);

  if (ec == std::errc()) {
    return value;
  }
  return defaultValue;
}

float safeStringToFloat(const char* str, float defaultValue) {
  if (!str || strlen(str) == 0) {
    return defaultValue;
  }
  try {
    return std::stof(str);
  } catch (const std::invalid_argument&) {
    return defaultValue;
  } catch (const std::out_of_range&) {
    return defaultValue;
  }
}

bool safeStringEqual(const char* str, const char* target) {
  if (!str || !target) {
    return false;
  }
  return std::string(str) == std::string(target);
}

static void AddElement(TiXmlElement* parent, const char* name, const std::string& value) {
  auto element = new TiXmlElement(name);
  element->LinkEndChild(new TiXmlText(value.c_str()));
  parent->LinkEndChild(element);
}

static std::string formatFloat(float value, int precision) {
  std::ostringstream oss;
  if (precision == 0 && value == static_cast<int>(value)) {
    oss << static_cast<int>(value);
  } else {
    oss << std::fixed << std::setprecision(precision) << value;
  }
  return oss.str();
}

static const char* GetChildElementText(TiXmlElement* fatherElement, const char* childName) {
  TiXmlElement* childElement = fatherElement->FirstChildElement(childName);
  if (childElement == nullptr) {
    return nullptr;
  }
  return childElement->GetText();
}

static void ReadCommonConfig(TiXmlElement* commonElement, ConfigParam* configParam) {
  if (!commonElement) {
    return;
  }

  if (TiXmlElement* tagLevelElement = commonElement->FirstChildElement("tag-level")) {
    if (const auto tagModeText = GetChildElementText(tagLevelElement, "mode")) {
      if (safeStringEqual(tagModeText, "beta")) {
        configParam->tagMode = TagMode::Beta;
      } else if (safeStringEqual(tagModeText, "custom")) {
        configParam->tagMode = TagMode::Custom;
      } else {
        configParam->tagMode = TagMode::Stable;
      }
    }

    if (const char* customLevelText = GetChildElementText(tagLevelElement, "custom-level")) {
      configParam->exportTagLevel = safeStringToInt<uint16_t>(customLevelText, 1023);
    }
  }

  if (const auto imageQualityText = GetChildElementText(commonElement, "image-quality")) {
    configParam->imageQuality = safeStringToInt(imageQualityText, 80);
  }

  if (const auto imagePixelRatioText = GetChildElementText(commonElement, "image-pixel-ratio")) {
    configParam->imagePixelRatio = safeStringToFloat(imagePixelRatioText, 2.0f);
  }

  if (const auto enableLayerNameText = GetChildElementText(commonElement, "enable-layer-name")) {
    configParam->enableLayerName = safeStringToInt(enableLayerNameText, 1) != 0;
  }

  if (const auto enableCompressionPanelText =
          GetChildElementText(commonElement, "enable-compression-panel")) {
    configParam->enableCompressionPanel = safeStringToInt(enableCompressionPanelText, 0) != 0;
  }

  if (const auto enableFontFileText = GetChildElementText(commonElement, "enable-font-file")) {
    configParam->enableFontFile = safeStringToInt(enableFontFileText, 0) != 0;
  }

  if (const auto exportScenseText = GetChildElementText(commonElement, "export-scense")) {
    configParam->scenes = static_cast<ExportScenes>(safeStringToInt(exportScenseText, 0));
  }

  if (const auto exportLanguageText = GetChildElementText(commonElement, "export-language")) {
    configParam->language = static_cast<Language>(safeStringToInt(exportLanguageText, 0));
  }
}

static void ReadBitmapConfig(TiXmlElement* bitmapElement, ConfigParam* configParam) {
  if (!bitmapElement) {
    return;
  }

  if (const auto sequenceSuffixText = GetChildElementText(bitmapElement, "sequence-suffix")) {
    configParam->sequenceSuffix = sequenceSuffixText;
  }

  if (const auto sequenceTypeText = GetChildElementText(bitmapElement, "sequence-type")) {
    configParam->sequenceType =
        static_cast<pag::CompositionType>(safeStringToInt(sequenceTypeText, 1));
  }

  if (const auto sequenceQualityText = GetChildElementText(bitmapElement, "sequence-quality")) {
    configParam->sequenceQuality = safeStringToInt(sequenceQualityText, 80);
  }

  if (const auto keyframeIntervalText = GetChildElementText(bitmapElement, "keyframe-interval")) {
    configParam->bitmapKeyFrameInterval = safeStringToInt(keyframeIntervalText, 60);
  }

  if (const auto maxResolutionText = GetChildElementText(bitmapElement, "max-resolution")) {
    configParam->bitmapMaxResolution = safeStringToInt(maxResolutionText, 720);
  }

  if (TiXmlElement* sequencesElement = bitmapElement->FirstChildElement("sequences")) {
    if (const TiXmlElement* sequenceElement = sequencesElement->FirstChildElement("sequence")) {
      double frameRate = 24.0;
      if (sequenceElement->Attribute("frameRate", &frameRate)) {
        configParam->frameRate = static_cast<float>(frameRate);
      }
    }
  }

  if (configParam->frameRate <= 0) {
    configParam->frameRate = 24.0f;
  }
}

static void WriteCommonConfig(TiXmlElement* root, ConfigParam* configParam) {
  auto common = new TiXmlElement("common");
  root->LinkEndChild(common);

  auto tagLevel = new TiXmlElement("tag-level");
  common->LinkEndChild(tagLevel);

  auto mode = new TiXmlElement("mode");
  switch (configParam->tagMode) {
    case TagMode::Beta:
      mode->LinkEndChild(new TiXmlText("beta"));
      break;
    case TagMode::Custom:
      mode->LinkEndChild(new TiXmlText("custom"));
      break;
    default:
      mode->LinkEndChild(new TiXmlText("stable"));
      break;
  }
  tagLevel->LinkEndChild(mode);

  auto customLevel = new TiXmlElement("custom-level");
  customLevel->LinkEndChild(new TiXmlText(std::to_string(configParam->exportTagLevel).c_str()));
  tagLevel->LinkEndChild(customLevel);

  AddElement(common, "image-quality", std::to_string(configParam->imageQuality));
  AddElement(common, "image-pixel-ratio", formatFloat(configParam->imagePixelRatio, 1));
  AddElement(common, "enable-compression-panel",
             std::to_string(configParam->enableCompressionPanel ? 1 : 0));
  AddElement(common, "enable-layer-name", std::to_string(configParam->enableLayerName ? 1 : 0));
  AddElement(common, "enable-font-file", std::to_string(configParam->enableFontFile ? 1 : 0));
  AddElement(common, "export-scense", std::to_string(static_cast<int>(configParam->scenes)));
  AddElement(common, "export-language", std::to_string(static_cast<int>(configParam->language)));
}

static void WriteBitmapConfig(TiXmlElement* root, ConfigParam* configParam) {
  auto bitmap = new TiXmlElement("bitmap");
  root->LinkEndChild(bitmap);

  AddElement(bitmap, "sequence-suffix", configParam->sequenceSuffix);
  AddElement(bitmap, "sequence-type", std::to_string(static_cast<int>(configParam->sequenceType)));
  AddElement(bitmap, "sequence-quality", std::to_string(configParam->sequenceQuality));
  AddElement(bitmap, "keyframe-interval", std::to_string(configParam->bitmapKeyFrameInterval));
  AddElement(bitmap, "max-resolution", std::to_string(configParam->bitmapMaxResolution));

  auto sequences = new TiXmlElement("sequences");
  bitmap->LinkEndChild(sequences);

  auto sequence = new TiXmlElement("sequence");
  sequence->SetAttribute("scale", "1");
  sequence->SetAttribute("framerate", formatFloat(configParam->frameRate, 1).c_str());
  sequences->LinkEndChild(sequence);

  auto vector = new TiXmlElement("vector");
  auto vectorText = new TiXmlText("");
  vector->LinkEndChild(vectorText);
  root->LinkEndChild(vector);

  auto video = new TiXmlElement("video");
  auto videoText = new TiXmlText("");
  video->LinkEndChild(videoText);
  root->LinkEndChild(video);
}

int WriteDefaultConfigFile(const char* fileName) {
  if (!fileName) {
    return -1;
  }

  TiXmlDocument doc;
  auto decl = new TiXmlDeclaration("1.0", "utf-8", "");
  doc.LinkEndChild(decl);

  auto root = new TiXmlElement("pag-exporter");
  doc.LinkEndChild(root);

  ConfigParam defaultConfig;
  WriteCommonConfig(root, &defaultConfig);
  WriteBitmapConfig(root, &defaultConfig);

  if (!doc.SaveFile(fileName)) {
    return -1;
  }
  return 0;
}

bool ReadConfigFile(ConfigParam* configParam) {
  if (!configParam) return false;

  const std::string configPath = GetConfigPath();
  if (configPath.empty()) {
    printf("Failed to get config path.\n");
    return false;
  }

  std::string filename = configPath + "PAGConfig.xml";
  if (!std::filesystem::exists(filename)) {
    printf("Config file not found: %s, creating default config.\n", filename.c_str());
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
  auto decl = new TiXmlDeclaration("1.0", "utf-8", "");
  doc.LinkEndChild(decl);

  auto root = new TiXmlElement("pag-exporter");
  doc.LinkEndChild(root);

  WriteCommonConfig(root, configParam);
  WriteBitmapConfig(root, configParam);

  if (!doc.SaveFile(filename.c_str())) {
    printf("Failed to save config file: %s\n", filename.c_str());
  } else {
    printf("Config saved to: %s\n", filename.c_str());
  }
}
}  // namespace exporter
