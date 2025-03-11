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
#ifndef CONFIGPARAM_H
#define CONFIGPARAM_H
#include <pag/file.h>
#include <string>
#include "nlohmann/json.hpp"
enum class LanguageType {
  Chinese = 0,
  English = 1,
};

enum class ExportScenes {
  GeneralScene = 0,    // 通用场景
  UIScene = 1,         // UI场景
  VideoEditScene = 2,  // 动画编辑场景
};

enum class ExportVersionType {
  Stable = 0,
  Beta = 1,
  Custom = 2,
};

enum class FrameFormatType {
  h264 = 0,
  Webp = 1,
};

struct CommonParam {
  LanguageType language = LanguageType::Chinese;
  ExportScenes exportScene = ExportScenes::GeneralScene;
  ExportVersionType exportVersion = ExportVersionType::Stable;
  int tagLevel = 1023;
  int imageQuality = 80;
  float imagePixelRatio = 2.0;
  bool enableLayerName = true;
  bool enableFontFile = false;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(CommonParam, language, exportScene, exportVersion, tagLevel,
                                 imageQuality, imagePixelRatio, enableLayerName, enableFontFile)
};

struct BMPComposParam {
  FrameFormatType frameFormat = FrameFormatType::h264;
  int sequenceQuality = 80;
  int bitmapMaxResolution = 720;
  float maxFrameRate = 24.0;
  int bitmapKeyFrameInterval = 60;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(BMPComposParam, frameFormat, sequenceQuality, bitmapMaxResolution,
                                 maxFrameRate, bitmapKeyFrameInterval)
};

struct ConfigParam {
  CommonParam commonParam;
  BMPComposParam bmpComposParam;
  std::vector<std::pair<float, float>> scaleAndFpsList = {std::make_pair(1.00f, 24.0f)};
  pag::CompositionType sequenceType = pag::CompositionType::Video;
  std::string sequenceSuffix = "_bmp";
  bool enableCompressionPanel = false;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(ConfigParam, commonParam, bmpComposParam, scaleAndFpsList,
                                 sequenceType, sequenceSuffix, enableCompressionPanel)
};

class ConfigParamManager {

 public:
  static ConfigParamManager& getInstance();
  void init();
  bool getConfigParam(ConfigParam& configParam);
  bool setConfigParam(const ConfigParam& configParam);
  std::string getConfigFileSavePath();
  ConfigParamManager(const ConfigParamManager&) = delete;
  ConfigParamManager& operator=(const ConfigParamManager&) = delete;

  bool configFileIsExist() const;

 private:
  ConfigParamManager() {
    init();
  }
  ~ConfigParamManager()= default;
  ConfigParam configParam;
  std::string configFileSavePath;
};

std::string GetRoamingPath();
std::string GetConfigPath();

void CreateFolder(const std::string& path);
void ReCreateFolder(const std::string& path);

#endif  //CONFIGPARAM_H
