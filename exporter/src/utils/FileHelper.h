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

#pragma once
#include <filesystem>
#include <string>
namespace FileHelper {

std::string ReadTextFile(const std::string& filename);

int WriteTextFile(const std::string& fileName, const char* text);

int WriteTextFile(const std::string& fileName, const std::string& text);

size_t GetFileSize(const std::string& fileName);

bool CopyFile(const std::string& src, const std::string& dst);

bool FileIsExist(const std::string& fileName);

class ScopedTempFile {
 public:
  ScopedTempFile() = default;
  ~ScopedTempFile();

  void setFilePath(const std::string& path);

 private:
  std::string tempFilePath = "";
};

}  // namespace FileHelper
