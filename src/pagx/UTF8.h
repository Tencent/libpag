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

#pragma once

#include <cstddef>
#include <cstdint>

namespace pagx {

/**
 * Decodes one UTF-8 character from the given data. Returns the number of bytes consumed, or 0 if
 * the byte sequence is invalid.
 */
inline size_t DecodeUTF8Char(const char* data, size_t remaining, int32_t* unichar) {
  if (remaining == 0) {
    return 0;
  }
  auto byte = static_cast<uint8_t>(data[0]);
  int32_t codepoint = 0;
  size_t charLen = 0;

  if ((byte & 0x80) == 0) {
    codepoint = byte;
    charLen = 1;
  } else if ((byte & 0xE0) == 0xC0) {
    codepoint = byte & 0x1F;
    charLen = 2;
  } else if ((byte & 0xF0) == 0xE0) {
    codepoint = byte & 0x0F;
    charLen = 3;
  } else if ((byte & 0xF8) == 0xF0) {
    codepoint = byte & 0x07;
    charLen = 4;
  } else {
    return 0;
  }

  if (charLen > remaining) {
    return 0;
  }

  for (size_t j = 1; j < charLen; j++) {
    codepoint = (codepoint << 6) | (static_cast<uint8_t>(data[j]) & 0x3F);
  }

  *unichar = codepoint;
  return charLen;
}

}  // namespace pagx
