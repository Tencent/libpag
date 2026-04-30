/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include "renderer/ImageEmbedder.h"
#include <fstream>
#include <unordered_set>
#include "base/utils/Log.h"
#include "pagx/types/Data.h"

namespace pagx {

static constexpr size_t MaxFileSize = 256 * 1024 * 1024;

static std::shared_ptr<pagx::Data> ReadFileToData(const std::string& path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in.is_open()) return nullptr;
  std::streampos end = in.tellg();
  if (end <= 0) return nullptr;  // covers tellg failure and empty file
  auto size = static_cast<size_t>(end);
  if (size > MaxFileSize) {
    LOGE("ReadFileToData: file '%s' exceeds the maximum size limit (%zu bytes).", path.c_str(),
         MaxFileSize);
    return nullptr;
  }
  in.seekg(0, std::ios::beg);
  auto* buffer = new (std::nothrow) uint8_t[size];
  if (buffer == nullptr) return nullptr;
  in.read(reinterpret_cast<char*>(buffer), static_cast<std::streamsize>(size));
  if (!in || static_cast<size_t>(in.gcount()) != size) {
    delete[] buffer;
    return nullptr;
  }
  return pagx::Data::MakeAdopt(buffer, size);
}

bool ImageEmbedder::embed(PAGXDocument* document) {
  if (document == nullptr) return false;
  auto paths = document->getExternalFilePaths();
  std::unordered_set<std::string> loaded;
  for (const auto& path : paths) {
    if (!loaded.insert(path).second) {
      continue;
    }
    auto data = ReadFileToData(path);
    if (data == nullptr) {
      lastErrorPath_ = path;
      return false;
    }
    document->loadFileData(path, data);
  }
  return true;
}

}  // namespace pagx
