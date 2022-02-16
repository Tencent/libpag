/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <climits>
#include <cstddef>

namespace tgfx {
class ResourceHandle {
 public:
  ResourceHandle() = default;

  explicit ResourceHandle(size_t value) : value(value) {
  }

  bool isValid() const {
    return kInvalid_ResourceHandle != value;
  }

  size_t toIndex() const {
    return value;
  }

 private:
  static const size_t kInvalid_ResourceHandle = ULONG_MAX;
  size_t value = kInvalid_ResourceHandle;
};

class UniformHandle : public ResourceHandle {
 public:
  UniformHandle() = default;

  explicit UniformHandle(size_t value) : ResourceHandle(value) {
  }
};

class SamplerHandle : public ResourceHandle {
 public:
  SamplerHandle() = default;

  explicit SamplerHandle(size_t value) : ResourceHandle(value) {
  }
};
}  // namespace tgfx
