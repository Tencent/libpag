/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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
#include <string>
#include <vector>

namespace pag {
class Directory {
 public:
  static std::string GetFileName(const std::string& filePath);

  static std::string GetParentDirectory(const std::string& filePath);

  static std::string JoinPath(const std::string& folder, const std::string& file);

  static bool CreateRecursively(const std::string& folder);

  static void VisitFiles(const std::string& folder,
                         std::function<void(const std::string&, size_t)> callback);

  static std::vector<std::string> FindFiles(const std::string& folder, const std::string& ext);
};
}  // namespace pag
