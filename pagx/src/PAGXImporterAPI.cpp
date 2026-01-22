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

#include "pagx/PAGXImporter.h"
#include <cstdio>
#include "PAGXImporterImpl.h"

namespace pagx {

std::shared_ptr<PAGXDocument> PAGXImporter::FromFile(const std::string& filePath) {
  FILE* file = fopen(filePath.c_str(), "rb");
  if (!file) {
    return nullptr;
  }

  fseek(file, 0, SEEK_END);
  long fileSize = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (fileSize <= 0) {
    fclose(file);
    return nullptr;
  }

  std::string content;
  content.resize(static_cast<size_t>(fileSize));
  size_t bytesRead = fread(&content[0], 1, static_cast<size_t>(fileSize), file);
  fclose(file);

  if (bytesRead != static_cast<size_t>(fileSize)) {
    return nullptr;
  }

  auto doc = FromXML(content);
  if (doc) {
    auto lastSlash = filePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
      doc->basePath = filePath.substr(0, lastSlash + 1);
    }
  }
  return doc;
}

std::shared_ptr<PAGXDocument> PAGXImporter::FromXML(const std::string& xmlContent) {
  return FromXML(reinterpret_cast<const uint8_t*>(xmlContent.data()), xmlContent.size());
}

std::shared_ptr<PAGXDocument> PAGXImporter::FromXML(const uint8_t* data, size_t length) {
  return PAGXImporterImpl::Parse(data, length);
}

}  // namespace pagx
