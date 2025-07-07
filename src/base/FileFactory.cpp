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

#include <algorithm>
#include <unordered_map>
#include "pag/file.h"

namespace pag {

static std::mutex globalLocker = {};
static std::unordered_map<std::string, std::weak_ptr<File>> weakFileMap =
    std::unordered_map<std::string, std::weak_ptr<File>>();

static std::shared_ptr<File> FindFileByPath(const std::string& filePath) {
  std::lock_guard<std::mutex> autoLock(globalLocker);
  if (filePath.empty()) {
    return nullptr;
  }
  auto result = weakFileMap.find(filePath);
  if (result != weakFileMap.end()) {
    auto& weak = result->second;
    auto file = weak.lock();
    if (file) {
      return file;
    }
    weakFileMap.erase(result);
    if (weakFileMap.size() > 50) {  // do cleaning.
      std::vector<std::string> needRemoveList = {};
      for (auto& item : weakFileMap) {
        if (item.second.expired()) {
          needRemoveList.push_back(item.first);
        }
      }
      for (auto& item : needRemoveList) {
        weakFileMap.erase(item);
      }
    }
  }
  return nullptr;
}

std::shared_ptr<File> File::Load(const std::string& filePath, const std::string& password) {
  auto byteData = ByteData::FromPath(filePath);
  if (byteData == nullptr) {
    return nullptr;
  }
  return pag::File::Load(byteData->data(), byteData->length(), filePath, password);
}

std::shared_ptr<File> File::Load(const void* bytes, size_t length, const std::string& filePath,
                                 const std::string&) {
  auto file = FindFileByPath(filePath);
  if (file != nullptr) {
    return file;
  }
  file = Codec::Decode(bytes, static_cast<uint32_t>(length), filePath);
  if (file != nullptr) {
    std::lock_guard<std::mutex> autoLock(globalLocker);
    std::weak_ptr<File> weak = file;
    weakFileMap.insert(std::make_pair(filePath, std::move(weak)));
  }
  return file;
}

}  // namespace pag
