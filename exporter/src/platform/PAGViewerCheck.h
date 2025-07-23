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

#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace exporter {
enum class InstallResult {
  Success = 0,
  FileNotFound = -1,
  AccessDenied = -2,
  InvalidPath = -3,
  ExecutionFailed = -4,
  PermissionDenied = -5,
  UnknownError = -999
};

struct InstallStatus {
  InstallResult result;
  std::string errorMessage;
  bool isSuccess() const {
    return result == InstallResult::Success;
  }
  static InstallStatus Success() {
    return {InstallResult::Success, ""};
  }
  static InstallStatus Error(InstallResult result, const std::string& message) {
    return {result, message};
  }
};

struct SoftWareInfo {
  std::string displayName;
  std::string version;
  std::string installLocation;
  std::string uninstallString;
  std::string bundleID;
};

class CheckConfig {
 public:
  void SetTargetAppName(const std::string& name);
  std::string GetTargetAppName();
  void SetInstallerPath(const std::string& path);
  std::string GetInstallerPath();

  void SetPlatformSpecificConfig(const std::string& key, const std::string& value);
  std::string GetPlatformSpecificConfig(const std::string& key);

 private:
  std::string targetAppName = "PAGViewer";
  std::string installerPath;
  std::unordered_map<std::string, std::string> platformConfig;
};

class PAGViewerCheck {
 public:
  explicit PAGViewerCheck(std::shared_ptr<CheckConfig> config);

  bool IsPAGViewerInstalled();

  InstallStatus InstallPAGViewer();

  void setProgressCallback(std::function<void(int)> callback);

  std::vector<SoftWareInfo> FindSoftwareByName(const std::string& namePattern);

  SoftWareInfo GetPAGViewerInfo();

 private:
  std::shared_ptr<CheckConfig> config;
  std::function<void(int)> progressCallback;
};

}  // namespace exporter
