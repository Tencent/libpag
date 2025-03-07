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

#include "ConfigParam.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

ConfigParamManager& ConfigParamManager::getInstance() {
  static ConfigParamManager instance;
  return instance;
}

void ConfigParamManager::init() {
  getConfigFileSavePath();
  if (!fs::exists(configFileSavePath)) {
    ConfigParam param;
    setConfigParam(param);
  }
}

bool ConfigParamManager::getConfigParam(ConfigParam& param) {
  try {
    // 打开文件并读取 JSON 数据
    std::ifstream file(configFileSavePath);
    if (!file.is_open()) {
      std::cerr << "Failed to open file for reading: " << configFileSavePath << std::endl;
      return false;
    }

    nlohmann::json j;
    file >> j;
    file.close();
    configParam = j.get<ConfigParam>();
    param = configParam;
    return true;
  } catch (const std::exception& e) {
    std::cerr << "Error reading JSON from file: " << e.what() << std::endl;
    return false;
  }
}

bool ConfigParamManager::setConfigParam(const ConfigParam& param) {
  try {
    configParam = param;
    if (configParam.bmpComposParam.frameFormat == FrameFormatType::h264) {
      configParam.sequenceType = pag::CompositionType::Video;
    } else if (configParam.bmpComposParam.frameFormat == FrameFormatType::Webp) {
      configParam.sequenceType = pag::CompositionType::Bitmap;
    }
    configParam.scaleAndFpsList[0].second = configParam.bmpComposParam.maxFrameRate;
    nlohmann::json j = configParam;
    std::ofstream file(configFileSavePath);
    if (!file.is_open()) {
      std::cerr << "Failed to open file for writing: " << configFileSavePath << std::endl;
      return false;
    }
    file << std::setw(4) << j << std::endl;
    file.close();
    return true;
  } catch (const std::exception& e) {
    std::cerr << "Error writing JSON to file: " << e.what() << std::endl;
    return false;
  }
}

std::string ConfigParamManager::getConfigFileSavePath() {
  const std::string appDataPath = GetRoamingPath();
  const fs::path path = fs::path(appDataPath) / "PAGExporter";
  if (!fs::exists(path)) {
    fs::create_directories(path);
  }
  const fs::path filename = path / "Config.json";
  configFileSavePath = filename.string();
  return filename.string();
}

bool ConfigParamManager::configFileIsExist() const {
  if (fs::exists(configFileSavePath)) {
    return true;
  } else {
    return false;
  }
}

std::string GetRoamingPath() {
  std::string roamingPath;
#ifdef WIN32
  char* envVar = nullptr;
  size_t len = 0;
  _dupenv_s(&envVar, &len, "APPDATA");
  if (envVar) {
    roamingPath = envVar;
    free(envVar);
  } else {
    std::cerr << "Environment variable 'APPDATA' not found." << std::endl;
  }
#elif defined(__APPLE__) || defined(__MACH__)
  roamingPath = std::getenv("HOME");
  roamingPath += "/Library/Application Support";
#endif
  return fs::path(roamingPath).string();
}

std::string GetConfigPath() {
  const fs::path path = fs::path(GetRoamingPath()) / "PAGExporter/";
  return path.string();
}

void CreateFolder(const std::string& path) {
  if (!fs::exists(path)) {
    fs::create_directories(path);
  }
}

void ReCreateFolder(const std::string& path) {
  if (fs::exists(path)) {
    fs::remove_all(path);
  }
  fs::create_directories(path);
}