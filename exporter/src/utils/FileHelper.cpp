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

#include "FileHelper.h"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs = std::filesystem;

namespace FileHelper {

std::string ReadTextFile(const std::string& filename) {
  std::ifstream inputStream(filename);
  if (!inputStream.is_open()) {
    return "";
  }
  std::string text((std::istreambuf_iterator<char>(inputStream)),
                   (std::istreambuf_iterator<char>()));
  inputStream.close();
  return text;
}

int WriteTextFile(const std::string& fileName, const char* text) {
  std::ofstream file(fileName, std::ios::out);
  if (!file.is_open()) {
    return 0;
  }
  file << text;
  file.close();
  return file.tellp();
}

int WriteTextFile(const std::string& fileName, const std::string& text) {
  return WriteTextFile(fileName, text.c_str());
}

size_t GetFileSize(const std::string& fileName) {
  try {
    return fs::file_size(fileName);
  } catch (...) {
    return 0;
  }
}

bool FileIsExist(const std::string& fileName) {
  try {
    if (fileName.empty()) {
      return false;
    }
    return fs::exists(fileName);
  } catch (...) {
    return false;
  }
}

bool CopyFile(const std::string& src, const std::string& dst) {
  try {
    if (!FileIsExist(src)) {
      return false;
    }
    auto parentPath = fs::path(dst).parent_path();
    if (!FileIsExist(parentPath)) {
      fs::create_directories(parentPath);
    }
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
    return true;
  } catch (...) {
    return false;
  }
}

ScopedTempFile::~ScopedTempFile() {
  if (!tempFilePath.empty() && std::filesystem::exists(tempFilePath)) {
    std::remove(tempFilePath.c_str());
  }
}

void ScopedTempFile::setFilePath(const std::string& path) {
  tempFilePath = path;
}

}  // namespace FileHelper
