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

#include "tgfx/core/BytesKey.h"
#include <cstring>

namespace tgfx {
union DataConverter {
  float floatValue;
  uint8_t bytes[4];
  uint32_t uintValue;
};

union PointerConverter {
  const void* pointer;
  uint32_t uintValues[2];
};

void BytesKey::write(uint32_t value) {
  values.push_back(value);
}

void BytesKey::write(const void* value) {
  PointerConverter converter = {};
  converter.pointer = value;
  values.push_back(converter.uintValues[0]);
  static size_t size = sizeof(intptr_t);
  if (size > 4) {
    values.push_back(converter.uintValues[1]);
  }
}

void BytesKey::write(const uint8_t value[4]) {
  DataConverter converter = {};
  memcpy(converter.bytes, value, 4);
  values.push_back(converter.uintValue);
}

void BytesKey::write(float value) {
  DataConverter converter = {};
  converter.floatValue = value;
  values.push_back(converter.uintValue);
}

size_t BytesHasher::operator()(const BytesKey& key) const {
  auto hash = key.values.size();
  for (auto& value : key.values) {
    hash ^= value + 0x9e3779b9 + (hash << 6u) + (hash >> 2u);
  }
  return hash;
}
}  // namespace tgfx