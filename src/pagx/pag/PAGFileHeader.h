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

#pragma once

#include <cstdint>
#include <string>
#include "codec/utils/EncodeStream.h"

namespace pagx {

// The 16-byte PAG file header, byte-identical in layout to the legacy format
// (spec/pag_binary_spec.md §1.1): 'P' 'A' 'G' + version(u8) + bodyLength(u32 LE) +
// compression(i8, 'U' for the new format) + 7 reserved bytes. The new format always
// writes version 4.
struct PAGFileHeader {
  uint8_t version = 0;
  uint32_t bodyLength = 0;
  int8_t compression = 0;

  /**
   * Parses a header from the first bytes of a file. Returns false when fewer than 16 bytes
   * are available or the magic does not match.
   */
  bool parse(const uint8_t* bytes, size_t length);
};

/**
 * Writes a version-4 file header with the given body length. The body length must fit a
 * u32; the exporter checks the 4 GB limit before calling this (spec §1.1).
 */
void WritePAGFileHeader(pag::EncodeStream* stream, uint32_t bodyLength);

}  // namespace pagx
