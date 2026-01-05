/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "ProjectPath.h"
#include <filesystem>

namespace pag {
static std::string GetRootPath() {
  std::filesystem::path filePath = __FILE__;
  auto dir = filePath.parent_path().string();
  return std::filesystem::path(dir + "/../../..").lexically_normal();
}

std::string ProjectPath::Absolute(const std::string& relativePath) {
  std::filesystem::path path = relativePath;
  if (path.is_absolute()) {
    return path;
  }
  static const std::string rootPath = GetRootPath() + "/";
  return std::filesystem::path(rootPath + relativePath).lexically_normal();
}

}  // namespace pag
