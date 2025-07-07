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

#include "DevicePool.h"
#include <thread>
#include <unordered_map>

namespace pag {
thread_local std::shared_ptr<tgfx::GLDevice> cachedDevice = nullptr;

std::shared_ptr<tgfx::GLDevice> DevicePool::Make() {
  auto device = cachedDevice;
  if (device == nullptr) {
    device = tgfx::GLDevice::Make();
    cachedDevice = device;
  }
  return device;
}
}  // namespace pag
