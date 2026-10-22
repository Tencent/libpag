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
//  or required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/PAGImporter.h"
#include <fstream>
#include "pagx/pag/PAGFileHeader.h"

namespace pagx {
static PAGFormat DetectPAGFormatBytes(const uint8_t* bytes, size_t length) {
  PAGFileHeader header = {};
  if (header.parse(bytes, length)) {
    if (header.version <= 3) {
      return PAGFormat::PAGLegacy;
    }
    if (header.version == 4) {
      return PAGFormat::PAGv4;
    }
    return PAGFormat::Unknown;
  }
  size_t offset = 0;
  if (length >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
    offset = 3;
  }
  while (offset < length) {
    auto c = bytes[offset];
    if (c == '<') {
      return PAGFormat::PAGX;
    }
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
      break;
    }
    offset++;
  }
  return PAGFormat::Unknown;
}

PAGFormat PAGImporter::DetectFormat(const void* bytes, size_t length) {
  if (bytes == nullptr || length == 0) {
    return PAGFormat::Unknown;
  }
  return DetectPAGFormatBytes(static_cast<const uint8_t*>(bytes), length);
}

PAGFormat PAGImporter::DetectFormat(const std::string& filePath) {
  std::ifstream in(filePath, std::ios::binary);
  uint8_t header[16] = {};
  in.read(reinterpret_cast<char*>(header), sizeof(header));
  auto length = static_cast<size_t>(in.gcount());
  return DetectPAGFormatBytes(header, length);
}
}  // namespace pagx
