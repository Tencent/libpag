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

#include <cinttypes>
#include <cmath>

namespace pag {

enum class ByteOrder { LittleEndian, BigEndian };

ByteOrder EndianTest();

static const ByteOrder NATIVE_BYTE_ORDER = EndianTest();
static const uint8_t LENGTH_FOR_STORE_NUM_BITS = 5;

union Bit8 {
  int8_t intValue;
  uint8_t uintValue;
  bool boolValue;
};
union Bit16 {
  uint8_t bytes[2];
  int16_t intValue;
  uint16_t uintValue;
};

union Bit32 {
  uint8_t bytes[4];
  int32_t intValue;
  uint32_t uintValue;
  float floatValue;
};

union Bit64 {
  uint8_t bytes[8];
  int64_t intValue;
  uint64_t uintValue;
  double doubleValue;
};

inline uint32_t BitsToBytes(uint64_t capacity) {
  return static_cast<uint32_t>(ceil(capacity * 0.125));
}
}  // namespace pag