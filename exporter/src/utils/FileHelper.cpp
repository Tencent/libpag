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

#include "FileHelper.h"
#include <QDesktopServices>
#include <QDir>
#include <QUrl>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "src/base/utils/Log.h"

namespace fs = std::filesystem;

namespace exporter {

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

size_t WriteTextFile(const std::string& fileName, const char* text) {
  auto path = Utf8ToPath(fileName);
  auto parentPath = path.parent_path();
  if (!parentPath.empty() && !fs::exists(parentPath)) {
    fs::create_directories(parentPath);
  }
  std::ofstream file(path, std::ios::out);
  if (!file.is_open()) {
    return 0;
  }
  file << text;
  auto size = static_cast<size_t>(file.tellp());
  file.close();
  return size;
}

size_t WriteTextFile(const std::string& fileName, const std::string& text) {
  return WriteTextFile(fileName, text.c_str());
}

size_t GetFileSize(const std::string& fileName) {
  if (!FileIsExist(fileName)) {
    return 0;
  }
  std::error_code ec;
  auto ret = fs::file_size(Utf8ToPath(fileName), ec);
  return ec ? 0 : ret;
}

bool FileIsExist(const std::string& fileName) {
  if (fileName.empty()) {
    return false;
  }
  std::error_code ec;
  return fs::exists(Utf8ToPath(fileName), ec);
}

bool CopyFile(const std::string& src, const std::string& dst) {
  try {
    auto srcPath = Utf8ToPath(src);
    if (!fs::exists(srcPath)) {
      return false;
    }
    auto dstPath = Utf8ToPath(dst);
    auto parentPath = dstPath.parent_path();
    if (!parentPath.empty() && !fs::exists(parentPath)) {
      fs::create_directories(parentPath);
    }
    fs::copy_file(srcPath, dstPath, fs::copy_options::overwrite_existing);
    return true;
  } catch (...) {
    return false;
  }
}

bool WriteToFile(const std::string& filePath, const char* data, std::streamsize size,
                 std::ios::openmode mode) {
  auto path = Utf8ToPath(filePath);
  auto parentPath = path.parent_path();
  if (!parentPath.empty() && !fs::exists(parentPath)) {
    fs::create_directories(parentPath);
  }
  std::ofstream file(path, mode);
  if (!file) {
    LOGE("Failed to open file: %s", filePath.c_str());
    return false;
  }

  file.write(data, size);

  if (file.fail()) {
    LOGE("Failed to write to file: %s", filePath.c_str());
    if (file.eof()) {
      LOGE("eof is set: End-of-File reached on input operation.");
    }
    file.close();
    return false;
  }

  file.close();
  return true;
}

bool DeleteFile(const std::string& path) {
  return fs::remove_all(path);
}

bool CreateDir(const std::string& path) {
  return fs::create_directories(path);
}

int ReadFileData(const std::string& filePath, uint8_t* buf, size_t bufSize) {
  std::ifstream file(filePath, std::ios::binary | std::ios::ate);
  if (!file) {
    return 0;
  }

  size_t fileSize = file.tellg();
  file.seekg(0);
  file.read(reinterpret_cast<char*>(buf), std::min(fileSize, bufSize));
  if (fileSize < bufSize) {
    buf[fileSize] = '\0';
  }
  return std::min(fileSize, bufSize);
}

std::string GetFileName(const std::string& filePath) {
  return PathToUtf8(Utf8ToPath(filePath).filename());
}

std::string GetDir(const std::string& filePath) {
  return PathToUtf8(Utf8ToPath(filePath).parent_path());
}

void OpenPAGFile(const std::string& filePath) {
  QDesktopServices::openUrl(QUrl::fromLocalFile(filePath.data()));
}

}  // namespace exporter
