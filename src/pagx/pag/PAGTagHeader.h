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

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/EncodeStream.h"
#include "pagx/pag/TagCode.h"

namespace pagx {

// Tag header IO (spec/pag_binary_spec.md §2): a uint16 with the 10-bit code in the high
// bits and a 6-bit length, lengths of 63 or more extended by a u32. Byte-identical to the
// legacy layout, but bound to the v4 TagCode space.

struct PAGTagHeader {
  TagCode code = TagCode::End;
  uint32_t length = 0;
};

/**
 * Writes a tag header. The length must be the exact byte size of the tag content.
 */
void WritePAGTagHeader(pag::EncodeStream* stream, TagCode code, uint32_t length);

/**
 * Reads a tag header and updates the decoding context's tag level statistics.
 */
PAGTagHeader ReadPAGTagHeader(pag::DecodeStream* stream);

// String table IO (spec §6.2): count varuint, then per entry a varuint byte length and
// raw UTF-8 bytes. The whole file (main document plus sub-documents) shares one table.

/**
 * Writes a string table from the collected strings, in collection order.
 */
void WritePAGStringTable(pag::EncodeStream* stream, const std::vector<std::string>& strings);

/**
 * Reads a string table. Returns false on malformed input.
 */
bool ReadPAGStringTable(pag::DecodeStream* stream, std::vector<std::string>* strings);

}  // namespace pagx
