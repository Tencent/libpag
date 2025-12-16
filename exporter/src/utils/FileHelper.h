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
#include "platform/PlatformHelper.h"

namespace exporter {

std::string ReadTextFile(const std::string& filename);

size_t WriteTextFile(const std::string& fileName, const char* text);

size_t WriteTextFile(const std::string& fileName, const std::string& text);

size_t GetFileSize(const std::string& fileName);

bool CopyFile(const std::string& src, const std::string& dst);

bool FileIsExist(const std::string& fileName);

bool WriteToFile(const std::string& filePath, const char* data, std::streamsize size,
                 std::ios::openmode mode = std::ios::out | std::ios::binary);

bool DeleteFile(const std::string& path);

bool CreateDir(const std::string& path);

int ReadFileData(const std::string& filePath, uint8_t* buf, size_t bufSize);

std::string GetFileName(const std::string& filePath);

std::string GetDir(const std::string& filePath);

void OpenPAGFile(const std::string& filePath);

template <typename... Args>
std::string JoinPaths(const std::string& first, Args&&... args) {
  std::filesystem::path result = Utf8ToPath(first);
  (void)std::initializer_list<int>{(result /= Utf8ToPath(std::forward<Args>(args)), 0)...};
  result = result.lexically_normal();
  return PathToUtf8(result);
}

}  // namespace exporter
