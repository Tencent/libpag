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

#include "SimpleArray.h"

namespace pag {
SimpleArray::SimpleArray(uint32_t capacity) {
  this->capacity = capacity;
  bytes = new uint8_t[capacity];
}

SimpleArray::~SimpleArray() {
  delete[] bytes;
}

void SimpleArray::setOrder(ByteOrder order) {
  _order = order;
}

std::unique_ptr<ByteData> SimpleArray::release() {
  auto data = ByteData::MakeAdopted(bytes, _length);
  capacity = 256;
  _position = 0;
  _length = 0;
  bytes = new uint8_t[capacity];
  return data;
}

void SimpleArray::writeUint8(uint8_t value) {
  ensureCapacity(_position + 1);
  bytes[_position++] = value;
  positionChanged();
}

void SimpleArray::writeInt32(int32_t value) {
  ensureCapacity(_position + 4);
  if (_order == NATIVE_BYTE_ORDER) {
    for (int i = 0; i < 4; i++) {
      bytes[_position++] = value >> i * 8;
    }
  } else {
    for (int i = 3; i >= 0; i--) {
      bytes[_position++] = value >> i * 8;
    }
  }
  positionChanged();
}

void SimpleArray::writeBytes(uint8_t* byteArray, uint32_t length, uint32_t offset) {
  ensureCapacity(_position + length);
  memcpy(bytes + _position, byteArray + offset, length);
  _position += length;
  positionChanged();
}

void SimpleArray::ensureCapacity(uint32_t length) {
  if (length > capacity) {
    expandCapacity(length);
  }
}

void SimpleArray::expandCapacity(uint32_t length) {
  while (capacity < length) {
    capacity = static_cast<uint32_t>(capacity * 1.5);
  }
  auto newBytes = new uint8_t[capacity];
  memcpy(newBytes, bytes, _length);
  delete[] bytes;
  bytes = newBytes;
}

void SimpleArray::positionChanged() {
  if (_position > _length) {
    _length = _position;
  }
}
}  // namespace pag
