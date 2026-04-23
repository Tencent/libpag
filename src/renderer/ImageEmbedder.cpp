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
#include "pagx/types/Data.h"

namespace pagx {

static std::shared_ptr<pagx::Data> ReadFileToData(const std::string& path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in.is_open()) return nullptr;
  std::streampos end = in.tellg();
  if (end <= 0) return nullptr;  // covers tellg failure and empty file
  auto size = static_cast<size_t>(end);
  in.seekg(0, std::ios::beg);
  auto* buffer = new uint8_t[size];
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
  for (const auto& path : paths) {
    if (path.find("://") != std::string::npos) {
      continue;  // URL per D1.3 — silently skip
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
