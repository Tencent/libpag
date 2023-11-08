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

#include "DevicePool.h"
#include <thread>
#include <unordered_map>

namespace pag {
static std::mutex threadCacheLocker = {};
static std::unordered_map<std::thread::id, std::shared_ptr<tgfx::GLDevice>> threadCacheMap = {};

std::shared_ptr<tgfx::GLDevice> DevicePool::Make() {
  std::lock_guard<std::mutex> autoLock(threadCacheLocker);
  auto threadID = std::this_thread::get_id();
  auto result = threadCacheMap.find(threadID);
  if (result != threadCacheMap.end()) {
    return result->second;
  }
  auto device = tgfx::GLDevice::Make();
  if (device != nullptr) {
    threadCacheMap[threadID] = device;
  }
  return device;
}

void DevicePool::CleanAll() {
  std::lock_guard<std::mutex> autoLock(threadCacheLocker);
  threadCacheMap = {};
}
}  // namespace pag
