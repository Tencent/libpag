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

// Returns the number of bytes consumed by the UTF-8 character starting at data[0]. Returns 1 for
// invalid sequences so the caller can advance past them.
inline size_t UTF8CharLength(const char* data, size_t remaining) {
  if (remaining == 0) {
    return 0;
  }
  auto byte = static_cast<uint8_t>(data[0]);
  if (byte < 0x80) {
    return 1;
  }
  if ((byte & 0xE0) == 0xC0 && remaining >= 2) {
    if ((static_cast<uint8_t>(data[1]) & 0xC0) == 0x80) {
      return 2;
    }
    return 1;
  }
  if ((byte & 0xF0) == 0xE0 && remaining >= 3) {
    if ((static_cast<uint8_t>(data[1]) & 0xC0) == 0x80 &&
        (static_cast<uint8_t>(data[2]) & 0xC0) == 0x80) {
      return 3;
    }
    return 1;
  }
  if ((byte & 0xF8) == 0xF0 && remaining >= 4) {
    if ((static_cast<uint8_t>(data[1]) & 0xC0) == 0x80 &&
        (static_cast<uint8_t>(data[2]) & 0xC0) == 0x80 &&
        (static_cast<uint8_t>(data[3]) & 0xC0) == 0x80) {
      return 4;
    }
    return 1;
  }
  return 1;
}

// Decodes a UTF-8 sequence into a Unicode code point. Returns the number of bytes consumed. Sets
// codepoint to U+FFFD for invalid, overlong, or surrogate sequences.
inline size_t DecodeUTF8(const char* data, size_t length, int32_t* codepoint) {
  if (length == 0) {
    *codepoint = 0;
    return 0;
  }
  auto byte = static_cast<uint8_t>(data[0]);
  if (byte < 0x80) {
    *codepoint = byte;
    return 1;
  }
  if ((byte & 0xE0) == 0xC0 && length >= 2 &&
      (static_cast<uint8_t>(data[1]) & 0xC0) == 0x80) {
    auto cp = ((byte & 0x1F) << 6) | (static_cast<uint8_t>(data[1]) & 0x3F);
    if (cp >= 0x80) {
      *codepoint = cp;
      return 2;
    }
    *codepoint = 0xFFFD;
    return 1;
  }
  if ((byte & 0xF0) == 0xE0 && length >= 3 &&
      (static_cast<uint8_t>(data[1]) & 0xC0) == 0x80 &&
      (static_cast<uint8_t>(data[2]) & 0xC0) == 0x80) {
    auto cp = ((byte & 0x0F) << 12) | ((static_cast<uint8_t>(data[1]) & 0x3F) << 6) |
              (static_cast<uint8_t>(data[2]) & 0x3F);
    if (cp >= 0x800 && (cp < 0xD800 || cp > 0xDFFF)) {
      *codepoint = cp;
      return 3;
    }
    *codepoint = 0xFFFD;
    return 1;
  }
  if ((byte & 0xF8) == 0xF0 && length >= 4 &&
      (static_cast<uint8_t>(data[1]) & 0xC0) == 0x80 &&
      (static_cast<uint8_t>(data[2]) & 0xC0) == 0x80 &&
      (static_cast<uint8_t>(data[3]) & 0xC0) == 0x80) {
    auto cp = ((byte & 0x07) << 18) | ((static_cast<uint8_t>(data[1]) & 0x3F) << 12) |
              ((static_cast<uint8_t>(data[2]) & 0x3F) << 6) |
              (static_cast<uint8_t>(data[3]) & 0x3F);
    if (cp >= 0x10000) {
      *codepoint = cp;
      return 4;
    }
    *codepoint = 0xFFFD;
    return 1;
  }
  *codepoint = 0xFFFD;
  return 1;
}

}  // namespace pagx
