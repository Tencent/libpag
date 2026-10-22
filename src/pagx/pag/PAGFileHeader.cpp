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
//  license is distributed on an "AS IS" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/pag/PAGFileHeader.h"
#include <cstring>

namespace pagx {
static constexpr char PAGMagic[3] = {'P', 'A', 'G'};
static constexpr size_t PAGHeaderSize = 16;

bool PAGFileHeader::parse(const uint8_t* bytes, size_t length) {
  if (bytes == nullptr || length < PAGHeaderSize) {
    return false;
  }
  if (bytes[0] != PAGMagic[0] || bytes[1] != PAGMagic[1] || bytes[2] != PAGMagic[2]) {
    return false;
  }
  version = bytes[3];
  std::memcpy(&bodyLength, bytes + 4, sizeof(uint32_t));
  compression = static_cast<int8_t>(bytes[8]);
  return true;
}

void WritePAGFileHeader(pag::EncodeStream* stream, uint32_t bodyLength) {
  stream->writeUint8(static_cast<uint8_t>(PAGMagic[0]));
  stream->writeUint8(static_cast<uint8_t>(PAGMagic[1]));
  stream->writeUint8(static_cast<uint8_t>(PAGMagic[2]));
  stream->writeUint8(4);
  stream->writeUint32(bodyLength);
  stream->writeInt8('U');
  for (size_t i = 9; i < PAGHeaderSize; i++) {
    stream->writeUint8(0);
  }
}
}  // namespace pagx
