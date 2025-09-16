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

class TempFileDelete {
 public:
  ~TempFileDelete() {
    if (!tempFilePath.empty() && std::filesystem::exists(tempFilePath)) {
      std::remove(tempFilePath.c_str());
    }
  }

  void setFilePath(const std::string& path) {
    tempFilePath = path;
  }

 private:
  std::string tempFilePath = "";
};

}  // namespace exporter
