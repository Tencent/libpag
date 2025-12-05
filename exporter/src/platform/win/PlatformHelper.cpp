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
#include "platform/PlatformHelper.h"
#include <shlobj.h>
#include <windows.h>
#include <cstdlib>
#include <filesystem>
#include <string>
#include "platform/PAGViewerCheck.h"
#include "src/base/utils/Log.h"
#include "ui/WindowManager.h"
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
  if (!FileIsExist(path)) {
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
    TempFolderPath = RunScript("Folder.temp.fsName;");
  }
  return TempFolderPath;
}
std::string GetDownloadsPath() {
  wchar_t* path = nullptr;
  HRESULT hr = SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &path);

  if (SUCCEEDED(hr) && path) {
    int size = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, path, -1, &result[0], size, nullptr, nullptr);
    CoTaskMemFree(path);
    return result;
  }

  wchar_t userProfile[MAX_PATH] = {0};
  if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH)) {
    std::filesystem::path downloadsPath = std::filesystem::path(userProfile) / "Downloads";
    return downloadsPath.string();
  }

  wchar_t tempPath[MAX_PATH];
  if (GetTempPathW(MAX_PATH, tempPath)) {
    std::filesystem::path fallbackPath = std::filesystem::path(tempPath) / "Downloads";
    return fallbackPath.string();
  }
  return "";
}

bool IsAEWindowActive() {
  HWND foreground = GetForegroundWindow();
  if (!foreground) {
    return false;
  }

  char className[256] = {};
  GetClassNameA(foreground, className, sizeof(className));
  std::string name = className;
  return name.find_first_of("AE") == 0;
}

std::string GetPAGViewerPath() {
  auto configPath = GetRoamingPath() + "PAGViewerPath.txt";
  auto pagViewerPath = ReadTextFile(configPath.c_str());
  if (!pagViewerPath.empty() && FileIsExist(pagViewerPath)) {
    return pagViewerPath;
  }

  auto config = std::make_shared<AppConfig>();
  config->setAppName("PAGViewer");
  auto check = std::make_unique<PAGViewerCheck>(config);
  auto info = check->getPackageInfo();

  if (!info.installLocation.empty()) {
    std::filesystem::path viewerPath =
        std::filesystem::path(info.installLocation) / "PAGViewer.exe";
    if (FileIsExist(viewerPath.string())) {
      return viewerPath.string();
    }
  }

  return "";
}

std::string GetQmlPath() {
  return "";
}

static void StartPreview(const std::string& pagFilePath) {
  if (!FileIsExist(pagFilePath)) {
    QString errorMsg =
        QString::fromUtf8(Messages::FILE_NOT_EXIST) + QString::fromStdString(pagFilePath);
    WindowManager::GetInstance().showSimpleError(errorMsg);
    return;
  }

  auto pagViewerPath = GetPAGViewerPath();
  if (pagViewerPath.empty()) {
    QString errorMsg =
        QString::fromUtf8(Messages::PAGVIEWER_NOT_FOUND) + QString::fromStdString(pagFilePath);
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
    QString errorMsg =
        QString::fromUtf8(Messages::PREVIEW_LAUNCH_FAILED) + QString::number(GetLastError());
    WindowManager::GetInstance().showSimpleError(errorMsg);
  }
}

void PreviewPAGFile(std::string pagFilePath) {
  auto config = std::make_shared<AppConfig>();
  config->setAppName("PAGViewer");
  auto checker = std::make_unique<PAGViewerCheck>(config);

  if (!checker->isPAGViewerInstalled()) {
    bool installSuccess = WindowManager::GetInstance().showPAGViewerInstallDialog(pagFilePath);
    if (!installSuccess) {
      return;
    }
  }
  StartPreview(pagFilePath);
}

std::filesystem::path Utf8ToPath(const std::string& utf8) {
  return {QString::fromUtf8(utf8.c_str()).toStdWString()};
}

std::string PathToUtf8(const std::filesystem::path& path) {
  return QString::fromStdWString(path.wstring()).toUtf8().toStdString();
}

}  // namespace exporter
