/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "ByteBuffer.h"

namespace tgfx {

ByteOrder EndianTest() {
  int i = 1;
  auto p = static_cast<char*>(static_cast<void*>(&i));
  return (*p == 1) ? ByteOrder::LittleEndian : ByteOrder::BigEndian;
}

void ByteBuffer::setOrder(ByteOrder order) {
  _order = order;
}

void ByteBuffer::setPosition(uint32_t value) {
  if (value < _length) {
    _position = value;
    positionChanged();
  }
}

void ByteBuffer::skip(uint32_t numBytes) {
  if (_position < _length - numBytes + 1) {
    _position += numBytes;
    positionChanged();
  }
}

Bit8 ByteBuffer::readBit8() {
  Bit8 data = {};
  if (_position < _length) {
    data.uintValue = bytes[_position++];
    positionChanged();
  }
  return data;
}

Bit16 ByteBuffer::readBit16() {
  Bit16 data = {};
  if ((_length > 1) && (_position < _length - 1)) {
    if (_order == NATIVE_BYTE_ORDER) {
      data.bytes[0] = bytes[_position++];
      data.bytes[1] = bytes[_position++];
    } else {
      data.bytes[1] = bytes[_position++];
      data.bytes[0] = bytes[_position++];
    }
    positionChanged();
  }
  return data;
}

Bit32 ByteBuffer::readBit24() {
  Bit32 data = {};
  if ((_length > 2) && (_position < _length - 2)) {
    if (_order == NATIVE_BYTE_ORDER) {
      for (int i = 0; i < 3; i++) {
        data.bytes[i] = bytes[_position++];
      }
    } else {
      for (int i = 3; i >= 1; i--) {
        data.bytes[i] = bytes[_position++];
      }
    }
    positionChanged();
  }
  return data;
}

Bit32 ByteBuffer::readBit32() {
  Bit32 data = {};
  if ((_length > 3) && (_position < _length - 3)) {
    if (_order == NATIVE_BYTE_ORDER) {
      for (int i = 0; i < 4; i++) {
        data.bytes[i] = bytes[_position++];
      }
    } else {
      for (int i = 3; i >= 0; i--) {
        data.bytes[i] = bytes[_position++];
      }
    }
    positionChanged();
  }
  return data;
}

}  // namespace tgfx
