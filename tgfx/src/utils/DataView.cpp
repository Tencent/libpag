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

#include "tgfx/utils/DataView.h"
#include "utils/Log.h"

namespace tgfx {
static ByteOrder EndianTest() {
  int i = 1;
  auto p = static_cast<char*>(static_cast<void*>(&i));
  return (*p == 1) ? ByteOrder::LittleEndian : ByteOrder::BigEndian;
}

static const ByteOrder NATIVE_BYTE_ORDER = EndianTest();

union Bit8 {
  uint8_t bytes[1];
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

DataView::DataView(const uint8_t* buffer, size_t length, ByteOrder byteOrder)
    : _bytes(buffer), _size(length), _byteOrder(byteOrder) {
  if (_bytes == nullptr || _size == 0) {
    reset();
  }
}

DataView::DataView(uint8_t* buffer, size_t length, ByteOrder byteOrder)
    : _bytes(buffer), _writableBytes(buffer), _size(length), _byteOrder(byteOrder) {
  if (_bytes == nullptr || _size == 0) {
    reset();
  }
}

void DataView::reset() {
  _bytes = nullptr;
  _writableBytes = nullptr;
  _size = 0;
}

void DataView::reset(const uint8_t* bytes, size_t length) {
  if (bytes == nullptr || length == 0) {
    reset();
  } else {
    _bytes = bytes;
    _writableBytes = nullptr;
    _size = length;
  }
}

void DataView::reset(uint8_t* bytes, size_t length) {
  if (bytes == nullptr || length == 0) {
    reset();
  } else {
    _bytes = bytes;
    _writableBytes = bytes;
    _size = length;
  }
}

bool DataView::readBoolean(size_t offset) const {
  Bit8 data = {};
  if (!readData(offset, data.bytes, 1)) {
    return false;
  }
  return data.uintValue != 0;
}

int8_t DataView::readInt8(size_t offset) const {
  Bit8 data = {};
  if (!readData(offset, data.bytes, 1)) {
    return 0;
  }
  return data.intValue;
}

uint8_t DataView::readUint8(size_t offset) const {
  Bit8 data = {};
  if (!readData(offset, data.bytes, 1)) {
    return 0;
  }
  return data.uintValue;
}

int16_t DataView::readInt16(size_t offset) const {
  Bit16 data = {};
  if (!readData(offset, data.bytes, 2)) {
    return 0;
  }
  return data.intValue;
}

uint16_t DataView::readUint16(size_t offset) const {
  Bit16 data = {};
  if (!readData(offset, data.bytes, 2)) {
    return 0;
  }
  return data.uintValue;
}

int32_t DataView::readInt32(size_t offset) const {
  Bit32 data = {};
  if (!readData(offset, data.bytes, 4)) {
    return 0;
  }
  return data.intValue;
}

uint32_t DataView::readUint32(size_t offset) const {
  Bit32 data = {};
  if (!readData(offset, data.bytes, 4)) {
    return 0;
  }
  return data.uintValue;
}

int64_t DataView::readInt64(size_t offset) const {
  Bit64 data = {};
  if (!readData(offset, data.bytes, 8)) {
    return 0;
  }
  return data.intValue;
}

uint64_t DataView::readUint64(size_t offset) const {
  Bit64 data = {};
  if (!readData(offset, data.bytes, 8)) {
    return 0;
  }
  return data.uintValue;
}

float DataView::readFloat(size_t offset) const {
  Bit32 data = {};
  if (!readData(offset, data.bytes, 4)) {
    return 0.0f;
  }
  return data.floatValue;
}

double DataView::readDouble(size_t offset) const {
  Bit64 data = {};
  if (!readData(offset, data.bytes, 8)) {
    return 0.0;
  }
  return data.doubleValue;
}

bool DataView::writeBoolean(size_t offset, bool value) {
  Bit8 data = {};
  data.boolValue = value;
  return writeData(offset, data.bytes, 1);
}

bool DataView::writeInt8(size_t offset, int8_t value) {
  Bit8 data = {};
  data.intValue = value;
  return writeData(offset, data.bytes, 1);
}

bool DataView::writeUint8(size_t offset, uint8_t value) {
  Bit8 data = {};
  data.uintValue = value;
  return writeData(offset, data.bytes, 1);
}

bool DataView::writeInt16(size_t offset, int16_t value) {
  Bit16 data = {};
  data.intValue = value;
  return writeData(offset, data.bytes, 2);
}

bool DataView::writeUint16(size_t offset, uint16_t value) {
  Bit16 data = {};
  data.uintValue = value;
  return writeData(offset, data.bytes, 2);
}

bool DataView::writeInt32(size_t offset, int32_t value) {
  Bit32 data = {};
  data.intValue = value;
  return writeData(offset, data.bytes, 4);
}

bool DataView::writeUint32(size_t offset, uint32_t value) {
  Bit32 data = {};
  data.uintValue = value;
  return writeData(offset, data.bytes, 4);
}

bool DataView::writeInt64(size_t offset, int64_t value) {
  Bit64 data = {};
  data.intValue = value;
  return writeData(offset, data.bytes, 8);
}

bool DataView::writeUint64(size_t offset, uint64_t value) {
  Bit64 data = {};
  data.uintValue = value;
  return writeData(offset, data.bytes, 8);
}

bool DataView::writeFloat(size_t offset, float value) {
  Bit32 data = {};
  data.floatValue = value;
  return writeData(offset, data.bytes, 4);
}

bool DataView::writeDouble(size_t offset, double value) {
  Bit64 data = {};
  data.doubleValue = value;
  return writeData(offset, data.bytes, 8);
}

bool DataView::readData(size_t offset, uint8_t* data, size_t byteSize) const {
  if (_size - offset < byteSize) {
    LOGE("DataView::readData(): End of bytes was encountered!");
    return false;
  }
  if (_byteOrder == NATIVE_BYTE_ORDER) {
    for (int i = 0; i < static_cast<int>(byteSize); i++) {
      data[i] = _bytes[offset++];
    }
  } else {
    for (int i = static_cast<int>(byteSize) - 1; i >= 0; i--) {
      data[i] = _bytes[offset++];
    }
  }
  return true;
}

bool DataView::writeData(size_t offset, const uint8_t* data, size_t byteSize) {
  if (_writableBytes == nullptr) {
    LOGE("DataView::writeData(): This DataView is read-only!");
    return false;
  }
  if (_size - offset < byteSize) {
    LOGE("DataView::writeData(): End of bytes was encountered!");
    return false;
  }
  if (_byteOrder == NATIVE_BYTE_ORDER) {
    for (int i = 0; i < static_cast<int>(byteSize); i++) {
      _writableBytes[offset++] = data[i];
    }
  } else {
    for (int i = static_cast<int>(byteSize) - 1; i >= 0; i--) {
      _writableBytes[offset++] = data[i];
    }
  }
  return true;
}

}  // namespace tgfx
