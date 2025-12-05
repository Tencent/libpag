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
#include <filesystem>
#include <string>

namespace exporter {

namespace Messages {
constexpr const char* FILE_NOT_EXIST = "文件不存在，无法预览：";
constexpr const char* PAGVIEWER_NOT_FOUND = "PAGViewer未安装或路径错误，无法预览：";
constexpr const char* PREVIEW_LAUNCH_FAILED = "无法启动PAGViewer预览：";
constexpr const char* FILE_PATH_ENCODING_ERROR = "文件路径编码错误，无法预览文件。";
constexpr const char* INVALID_FILE_PATH = "无效的文件路径，无法创建预览。";
constexpr const char* PAGVIEWER_OPEN_FAILED = "使用PAGViewer打开文件失败：";
constexpr const char* PAGVIEWER_NOT_FOUND_MAC = "PAGViewer.app not found.";
}  // namespace Messages

std::string GetRoamingPath();

std::string GetConfigPath();

std::string GetTempFolderPath();

bool IsAEWindowActive();

std::string GetDownloadsPath();

std::string GetPAGViewerPath();

std::string GetQmlPath();

void PreviewPAGFile(std::string pagFilePath);

std::filesystem::path Utf8ToPath(const std::string& utf8);

std::string PathToUtf8(const std::filesystem::path& path);

inline std::filesystem::path Utf8ToPath(const char* utf8) {
  return Utf8ToPath(utf8 ? std::string(utf8) : std::string());
}

}  // namespace exporter
