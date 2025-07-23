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
#include <windows.h>
#include <shlobj.h>
#include "platform/PlatformHelper.h"
#include <cstdlib>
#include <filesystem>
#include <string>
#include "src/base/utils/Log.h"
#include "utils/AEHelper.h"
#include "utils/FileHelper.h"
#include "platform/PAGViewerCheck.h"
#include "ui/WindowManager.h"

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
std::string GetDownloadsPath() {
  wchar_t* path = nullptr;
  HRESULT hr = SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &path);

  if (SUCCEEDED(hr) && path) {
    int size = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr,0,  nullptr, nullptr);
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, path, -1, &result[0], size, nullptr, nullptr);
    CoTaskMemFree(path);
    return result;
  }

  wchar_t userProfile[MAX_PATH];
  if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH)) {
    std::filesystem::path downloadsPath = std::filesystem::path(userProfile) / "Downloads";
    return downloadsPath.string();
  }

  return "C:\\Users\\Downloads";
}

std::string GetPAGViewerPath() {
  auto configPath = GetRoamingPath() + "PAGViewerPath.txt";
  auto pagViewerPath = FileHelper::ReadTextFile(configPath.c_str());
  if (!pagViewerPath.empty() && FileHelper::FileIsExist(pagViewerPath)) {
    return pagViewerPath;
  }

  auto config = std::make_shared<CheckConfig>();
  config->SetTargetAppName("PAGViewer");
  auto check = std::make_unique<PAGViewerCheck>(config);
  auto info = check->GetPAGViewerInfo();

  if (!info.installLocation.empty()) {
    std::filesystem::path viewerPath =
        std::filesystem::path(info.installLocation) / "PAGViewer.exe";
    if (FileHelper::FileIsExist(viewerPath.string())) {
      return viewerPath.string();
    }
  }

  return "";
}

static void ExecutePreviewLogic(const std::string& pagFilePath) {
  if(!FileHelper::FileIsExist(pagFilePath)) {
    QString errorMsg = QString::fromUtf8("文件不存在，无法预览：") + QString::fromStdString(pagFilePath);
    WindowManager::GetInstance().showSimpleError(errorMsg);
    return;
  }

  auto pagViewerPath = GetPAGViewerPath();
  if(pagViewerPath.empty()) {
    QString errorMsg = QString::fromUtf8("PAGViewer未安装或路径错误，无法预览：") + QString::fromStdString(pagFilePath);
    WindowManager::GetInstance().showSimpleError(errorMsg);
    return;
  }

  auto winCmd = "\"" + pagViewerPath + "\" \"" + pagFilePath + "\"";

  STARTUPINFOA si = {0};
  PROCESS_INFORMATION pi = {0};

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  ZeroMemory(&pi, sizeof(pi));

  if (CreateProcessA(NULL, (LPSTR)winCmd.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  } else {
    QString errorMsg = QString::fromUtf8("无法启动PAGViewer预览：") + QString::number(GetLastError());
    WindowManager::GetInstance().showSimpleError(errorMsg);
  }



}

void PreviewPAGFile(std::string pagFilePath) {
  auto config = std::make_shared<CheckConfig>();
  config->SetTargetAppName("PAGViewer");
  auto checker = std::make_unique<PAGViewerCheck>(config);

  if(!checker->IsPAGViewerInstalled()) {
    bool installSuccess = WindowManager::GetInstance().showPAGViewerInstallDialog(pagFilePath);
    if(!installSuccess) {
      return;
    }
  }
  ExecutePreviewLogic(pagFilePath);


}

}  // namespace exporter
