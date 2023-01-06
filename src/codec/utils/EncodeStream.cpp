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

#include "EncodeStream.h"
#include <cstring>

namespace pag {

EncodeStream::EncodeStream(StreamContext* context, uint32_t capacity) : context(context) {
  this->capacity = capacity;
  bytes = new uint8_t[capacity];
}

EncodeStream::~EncodeStream() {
  delete[] bytes;
}

ByteOrder EncodeStream::order() const {
  return _order;
}

void EncodeStream::setOrder(pag::ByteOrder order) {
  _order = order;
}

std::unique_ptr<ByteData> EncodeStream::release() {
  auto data = ByteData::MakeAdopted(bytes, _length);
  capacity = 256;
  _position = 0;
  _length = 0;
  _bitPosition = 0;
  bytes = new uint8_t[capacity];
  return data;
}

void EncodeStream::writeBoolean(bool value) {
  Bit8 data = {};
  data.boolValue = value;
  writeBit8(data);
}

void EncodeStream::writeInt8(int8_t value) {
  Bit8 data = {};
  data.intValue = value;
  writeBit8(data);
}

void EncodeStream::writeUint8(uint8_t value) {
  ensureCapacity(_position + 1);
  bytes[_position++] = value;
  positionChanged();
}

void EncodeStream::writeInt16(int16_t value) {
  Bit16 data = {};
  data.intValue = value;
  writeBit16(data);
}

void EncodeStream::writeUint16(uint16_t value) {
  Bit16 data = {};
  data.uintValue = value;
  writeBit16(data);
}

void EncodeStream::writeUint24(uint32_t value) {
  Bit32 data = {};
  data.uintValue = value;
  writeBit24(data);
}

void EncodeStream::writeInt32(int32_t value) {
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

void EncodeStream::writeUint32(uint32_t value) {
  Bit32 data = {};
  data.uintValue = value;
  writeBit32(data);
}

void EncodeStream::writeInt64(int64_t value) {
  Bit64 data = {};
  data.intValue = value;
  writeBit64(data);
}

void EncodeStream::writeUint64(uint64_t value) {
  Bit64 data = {};
  data.uintValue = value;
  writeBit64(data);
}

void EncodeStream::writeFloat(float value) {
  Bit32 data = {};
  data.floatValue = value;
  writeBit32(data);
}

void EncodeStream::writeDouble(double value) {
  Bit64 data = {};
  data.doubleValue = value;
  writeBit64(data);
}

void EncodeStream::writeBytes(EncodeStream* stream, uint32_t length, uint32_t offset) {
  if (stream->_length <= offset) {
    return;
  }
  if (length == 0) {
    length = stream->_length - offset;
  }
  writeBytes(stream->bytes, length, offset);
}

void EncodeStream::writeBytes(uint8_t* stream, uint32_t length, uint32_t offset) {
  ensureCapacity(_position + length);
  memcpy(bytes + _position, stream + offset, length);
  _position += length;
  positionChanged();
}

void EncodeStream::writeByteData(const pag::ByteData* byteData) {
  if (byteData == nullptr) {
    return;
  }
  auto length = static_cast<uint32_t>(byteData->length());
  writeEncodedUint32(length);
  writeBytes(byteData->data(), length);
}

void EncodeStream::writeUTF8String(const std::string& text) {
  auto textLength = text.size();
  ensureCapacity(static_cast<uint32_t>(_position + textLength + 1));
  memcpy(bytes + _position, text.c_str(), textLength + 1);
  _position += textLength + 1;
  positionChanged();
}

void EncodeStream::writeEncodedInt64(int64_t value) {
  uint64_t flag = value < 0 ? 1 : 0;
  auto data = static_cast<uint64_t>(value < 0 ? -value : value);
  data = (data << 1) | flag;
  writeEncodedUint64(data);
}

void EncodeStream::writeEncodedUint64(uint64_t value) {
  static const uint64_t valueMask = 127;
  static const uint8_t hasNext = 128;
  uint8_t byte = 0;
  for (int i = 0; i < 64; i += 7) {
    byte = static_cast<uint8_t>(value & valueMask);
    value >>= 7;
    if (value > 0) {
      byte |= hasNext;
    }
    ensureCapacity(_position + 1);
    bytes[_position++] = byte;
    positionChanged();
    if (value == 0) {
      break;
    }
  }
}

void EncodeStream::writeBits(int32_t value, uint8_t numBits) {
  auto data = static_cast<uint32_t>(value << (33 - numBits));
  data >>= 33 - numBits;
  if (value < 0) {
    data |= 1 << (numBits - 1);
  }
  writeUBits(data, numBits);
}

void EncodeStream::writeUBits(uint32_t value, uint8_t numBits) {
  static const uint8_t bitMasks[9] = {0, 1, 3, 7, 15, 31, 63, 127, 255};
  ensureCapacity(BitsToBytes(_bitPosition + numBits));
  while (numBits > 0) {
    auto bytePosition = static_cast<uint32_t>(_bitPosition * 0.125);
    auto bitPosition = static_cast<uint32_t>(_bitPosition % 8);
    auto& byte = bytes[bytePosition];
    byte &= bitMasks[bitPosition];
    auto bitLength = std::min(8 - bitPosition, static_cast<uint32_t>(numBits));
    auto bits = static_cast<uint8_t>(value) & bitMasks[bitLength];
    byte |= (bits << bitPosition);
    value >>= bitLength;
    numBits -= bitLength;
    _bitPosition += bitLength;
  }
  bitPositionChanged();
}

uint8_t GetBitLength(uint32_t data) {
  uint8_t length = 32;
  static const uint32_t mask = 1ull << 31;
  while (length > 1) {
    if ((data & mask) > 0) {
      break;
    }
    data <<= 1;
    length--;
  }
  return static_cast<uint8_t>(length);
}

uint8_t GetBitLength(int32_t value) {
  auto data = static_cast<uint32_t>(value < 0 ? -value : value);
  uint8_t length = GetBitLength(data);
  if (length >= 32) {
    length = 31;
  }
  return static_cast<uint8_t>(length + 1);
}

void EncodeStream::writeInt32List(const int32_t* values, uint32_t count) {
  if (count == 0) {
    writeUBits(0, LENGTH_FOR_STORE_NUM_BITS);
    return;
  }
  uint8_t bitLength = 1;
  for (uint32_t i = 0; i < count; i++) {
    auto length = GetBitLength(values[i]);
    if (bitLength < length) {
      bitLength = length;
    }
  }
  writeUBits(static_cast<uint32_t>(bitLength - 1), LENGTH_FOR_STORE_NUM_BITS);
  for (uint32_t i = 0; i < count; i++) {
    writeBits(values[i], bitLength);
  }
}

void EncodeStream::writeUint32List(const uint32_t* values, uint32_t count) {
  if (count == 0) {
    writeUBits(0, LENGTH_FOR_STORE_NUM_BITS);
    return;
  }
  uint8_t bitLength = 1;
  for (uint32_t i = 0; i < count; i++) {
    auto length = GetBitLength(values[i]);
    if (bitLength < length) {
      bitLength = length;
    }
  }
  writeUBits(static_cast<uint32_t>(bitLength - 1), LENGTH_FOR_STORE_NUM_BITS);
  for (uint32_t i = 0; i < count; i++) {
    writeUBits(values[i], bitLength);
  }
}

void EncodeStream::writeFloatList(const float* values, uint32_t count, float precision) {
  if (count == 0) {
    writeUBits(0, LENGTH_FOR_STORE_NUM_BITS);
    return;
  }
  auto scale = 1 / precision;
  auto list = new int32_t[count];
  for (uint32_t i = 0; i < count; i++) {
    list[i] = static_cast<int32_t>(roundf(values[i] * scale));
  }
  writeInt32List(list, count);
  delete[] list;
}

void EncodeStream::writePoint2DList(const Point* points, uint32_t count, float precision) {
  auto list = new float[count*2+1];
  for (uint32_t i = 0; i < count; i++) {
    list[i*2+0] = points[i].x;
    list[i*2+1] = points[i].y;
  }
  writeFloatList(list, count*2, precision);
  delete[] list;
}

void EncodeStream::writePoint3DList(const Point3D* points, uint32_t count, float precision) {
  auto list = new float[count*3+1];
  for (uint32_t i = 0; i < count; i++) {
    list[i*3+0] = points[i].x;
    list[i*3+1] = points[i].y;
    list[i*3+2] = points[i].z;
  }
  writeFloatList(list, count*3, precision);
  delete[] list;
}

void EncodeStream::expandCapacity(uint32_t length) {
  while (capacity < length) {
    capacity = static_cast<uint32_t>(capacity * 1.5);
  }
  auto newBytes = new uint8_t[capacity];
  memcpy(newBytes, bytes, _length);
  delete[] bytes;
  bytes = newBytes;
}

void EncodeStream::writeBit8(Bit8 data) {
  ensureCapacity(_position + 1);
  bytes[_position++] = data.uintValue;
  positionChanged();
}

void EncodeStream::writeBit16(Bit16 data) {
  ensureCapacity(_position + 2);
  if (_order == NATIVE_BYTE_ORDER) {
    bytes[_position++] = data.bytes[0];
    bytes[_position++] = data.bytes[1];
  } else {
    bytes[_position++] = data.bytes[1];
    bytes[_position++] = data.bytes[0];
  }
  positionChanged();
}

void EncodeStream::writeBit24(Bit32 data) {
  ensureCapacity(_position + 3);
  if (_order == NATIVE_BYTE_ORDER) {
    for (int i = 0; i < 3; i++) {
      bytes[_position++] = data.bytes[i];
    }
  } else {
    for (int i = 3; i >= 1; i--) {
      bytes[_position++] = data.bytes[i];
    }
  }
  positionChanged();
}

void EncodeStream::writeBit32(Bit32 data) {
  ensureCapacity(_position + 4);
  if (_order == NATIVE_BYTE_ORDER) {
    for (int i = 0; i < 4; i++) {
      bytes[_position++] = data.bytes[i];
    }
  } else {
    for (int i = 3; i >= 0; i--) {
      bytes[_position++] = data.bytes[i];
    }
  }
  positionChanged();
}

void EncodeStream::writeBit64(Bit64 data) {
  ensureCapacity(_position + 8);
  if (_order == NATIVE_BYTE_ORDER) {
    for (int i = 0; i < 8; i++) {
      bytes[_position++] = data.bytes[i];
    }
  } else {
    for (int i = 7; i >= 0; i--) {
      bytes[_position++] = data.bytes[i];
    }
  }
  positionChanged();
}
}  // namespace pag
