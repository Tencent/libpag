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

  InstallStatus();

  explicit InstallStatus(InstallResult result, std::string message = "")
      : result(result), errorMessage(std::move(message)) {
  }

  bool isSuccess() const {
    return result == InstallResult::UnknownError;
  }
};

struct PackageInfo {
  std::string displayName;
  std::string version;
  std::string installLocation;
  std::string uninstallString;
  std::string bundleID;
};

class AppConfig {
 public:
  void setAppName(const std::string& name);
  std::string getAppName();
  void setInstallerPath(const std::string& path);
  std::string getInstallerPath();

  void addConfig(const std::string& key, const std::string& value);
  std::string getPlatformSpecificConfig(const std::string& key);

 private:
  std::string AppName = "PAGViewer";
  std::string installerPath = "";
  std::unordered_map<std::string, std::string> platformConfig;
};

class PAGViewerCheck {
 public:
  explicit PAGViewerCheck(std::shared_ptr<AppConfig> config);

  bool isPAGViewerInstalled();

  InstallStatus installPAGViewer();

  void setProgressCallback(std::function<void(int)> callback);

  std::vector<PackageInfo> findPackageinfoByName(const std::string& namePattern);

  PackageInfo getPackageInfo();

 private:
  std::shared_ptr<AppConfig> config = nullptr;
  std::function<void(int)> progressCallback;
};

}  // namespace exporter
