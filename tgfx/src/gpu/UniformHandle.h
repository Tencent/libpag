/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <string>

namespace tgfx {
class UniformHandle {
 public:
  UniformHandle() = default;

  explicit UniformHandle(std::string name, int stageIndex = 0) : key(std::move(name)) {
    if (!key.empty()) {
      key += "_Stage" + std::to_string(stageIndex);
    }
  }

  bool isValid() const {
    return !key.empty();
  }

  std::string toKey() const {
    return key;
  }

 private:
  std::string key;
};
}  // namespace tgfx
