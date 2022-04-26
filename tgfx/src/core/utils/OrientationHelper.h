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

#pragma once

#include "tgfx/core/Orientation.h"

namespace tgfx {
/*
 * @param data           Buffer to read bytes from
 * @param isLittleEndian Output parameter
 *                       Indicates if the data is little endian
 *                       Is unaffected on false returns
 */
static inline bool is_valid_endian_marker(const uint8_t* data, bool* isLittleEndian) {
  // II indicates Intel (little endian) and MM indicates motorola (big endian).
  if (('I' != data[0] || 'I' != data[1]) && ('M' != data[0] || 'M' != data[1])) {
    return false;
  }

  *isLittleEndian = ('I' == data[0]);
  return true;
}

static inline uint32_t get_endian_int(const uint8_t* data, bool littleEndian) {
  if (littleEndian) {
    return (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | (data[0]);
  }

  return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]);
}

inline uint16_t get_endian_short(const uint8_t* data, bool littleEndian) {
  if (littleEndian) {
    return (data[1] << 8) | (data[0]);
  }

  return (data[0] << 8) | (data[1]);
}

static bool is_orientation_marker(const uint8_t* data, size_t data_length,
                                  Orientation* orientation) {
  bool littleEndian;
  // We need eight bytes to read the endian marker and the offset, below.
  if (data_length < 8 || !is_valid_endian_marker(data, &littleEndian)) {
    return false;
  }

  // Get the offset from the start of the marker.
  // Though this only reads four bytes, use a larger int in case it overflows.
  uint64_t offset = get_endian_int(data + 4, littleEndian);

  // Require that the marker is at least large enough to contain the number of entries.
  if (data_length < offset + 2) {
    return false;
  }
  uint32_t numEntries = get_endian_short(data + offset, littleEndian);

  // Tag (2 bytes), Datatype (2 bytes), Number of elements (4 bytes), Data (4 bytes)
  const uint32_t kEntrySize = 12;
  const auto max = static_cast<uint32_t>((data_length - offset - 2) / kEntrySize);
  numEntries = numEntries < max ? numEntries : max;

  // Advance the data to the start of the entries.
  data += offset + 2;

  const uint16_t kOriginTag = 0x112;
  const uint16_t kOriginType = 3;
  for (uint32_t i = 0; i < numEntries; i++, data += kEntrySize) {
    uint16_t tag = get_endian_short(data, littleEndian);
    uint16_t type = get_endian_short(data + 2, littleEndian);
    uint32_t count = get_endian_int(data + 4, littleEndian);
    if (kOriginTag == tag && kOriginType == type && 1 == count) {
      uint16_t val = get_endian_short(data + 8, littleEndian);
      if (0 < val && val <= static_cast<int>(Orientation::LeftBottom)) {
        *orientation = (Orientation)val;
        return true;
      }
    }
  }

  return false;
}

}  // namespace tgfx