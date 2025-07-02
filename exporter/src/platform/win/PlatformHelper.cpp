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
#include "platform/PlatformHelper.h"
#include <cstdlib>
#include <filesystem>
#include <string>
#include "src/base/utils/Log.h"
#include "utils/AEHelper.h"
#include "utils/FileHelper.h"

namespace fs = std::filesystem;
std::string TempFolderPath = "";

namespace exporter {
std::string GetRoamingPath() {
  std::string path = "";
  char* roamingPath = nullptr;
  size_t len;
  errno_t err = _dupenv_s(&roamingPath, &len, "APPDATA");
  if (roamingPath == nullptr) {
    return path;
  }

  path = roamingPath;
  free(roamingPath);
  return path + "\\";
}

bool CreateFolder(const std::string& path) {
  if (!FileHelper::FileIsExist(path)) {
    std::error_code errorCode;
    if (!fs::create_directories(path, errorCode)) {
      LOGE("Create %s failed: %s", path.c_str(), errorCode.message().c_str());
      return false;
    }
  }
  return true;
}

std::string GetConfigPath() {
  std::string path = GetRoamingPath();
  if (path.empty()) {
    return path;
  }

  path += "PAGExporter\\";
  if (!CreateFolder(path)) {
    return "";
  }
  return path;
}

std::string GetTempFolderPath() {
  if (TempFolderPath.empty()) {
    const auto& suites = AEHelper::GetSuites();
    auto pluginID = AEHelper::GetPluginID();
    TempFolderPath = AEHelper::RunScript(suites, pluginID, "Folder.temp.fsName;");
  }
  return TempFolderPath;
}

}  // namespace exporter
