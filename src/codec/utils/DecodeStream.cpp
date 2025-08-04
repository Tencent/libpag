/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "DecodeStream.h"
#include <cstring>

namespace pag {
void DecodeStream::setPosition(uint32_t value) {
  if (!checkEndOfFile(value)) {
    positionChanged(static_cast<size_t>(value) - _position);
  }
}

void DecodeStream::skip(uint32_t numBytes) {
  if (!checkEndOfFile(numBytes)) {
    positionChanged(numBytes);
  }
}

bool DecodeStream::readBoolean() {
  if (!checkEndOfFile(1)) {
    auto value = dataView.getBoolean(_position);
    positionChanged(1);
    return value;
  }
  return false;
}

int8_t DecodeStream::readInt8() {
  if (!checkEndOfFile(1)) {
    auto value = dataView.getInt8(_position);
    positionChanged(1);
    return value;
  }
  return 0;
}

uint8_t DecodeStream::readUint8() {
  if (!checkEndOfFile(1)) {
    auto value = dataView.getUint8(_position);
    positionChanged(1);
    return value;
  }
  return 0;
}

int16_t DecodeStream::readInt16() {
  if (!checkEndOfFile(2)) {
    auto value = dataView.getInt16(_position);
    positionChanged(2);
    return value;
  }
  return 0;
}

uint16_t DecodeStream::readUint16() {
  if (!checkEndOfFile(2)) {
    auto value = dataView.getUint16(_position);
    positionChanged(2);
    return value;
  }
  return 0;
}

int32_t DecodeStream::readInt32() {
  if (!checkEndOfFile(4)) {
    auto value = dataView.getInt32(_position);
    positionChanged(4);
    return value;
  }
  return 0;
}

uint32_t DecodeStream::readUint32() {
  if (!checkEndOfFile(4)) {
    auto value = dataView.getUint32(_position);
    positionChanged(4);
    return value;
  }
  return 0;
}

int64_t DecodeStream::readInt64() {
  if (!checkEndOfFile(8)) {
    auto value = dataView.getInt64(_position);
    positionChanged(8);
    return value;
  }
  return 0;
}

uint64_t DecodeStream::readUint64() {
  if (!checkEndOfFile(8)) {
    auto value = dataView.getUint64(_position);
    positionChanged(8);
    return value;
  }
  return 0;
}

float DecodeStream::readFloat() {
  if (!checkEndOfFile(4)) {
    auto value = dataView.getFloat(_position);
    positionChanged(4);
    return value;
  }
  return 0;
}

double DecodeStream::readDouble() {
  if (!checkEndOfFile(8)) {
    auto value = dataView.getDouble(_position);
    positionChanged(8);
    return value;
  }
  return 0;
}

DecodeStream DecodeStream::readBytes(uint32_t length) {
  if (!checkEndOfFile(length)) {
    DecodeStream stream(context, dataView.bytes() + _position, length);
    positionChanged(length);
    return stream;
  }
  return DecodeStream(context);
}

std::unique_ptr<ByteData> DecodeStream::readByteData() {
  auto length = readEncodedUint32();
  auto bytes = readBytes(length);
  // must check whether the byte data is valid. otherwise, memcpy() will crash.
  if (length == 0 || length > bytes.length() || context->hasException()) {
    return nullptr;
  }
  return ByteData::MakeCopy(bytes.data(), length);
}

std::string DecodeStream::readUTF8String() {
  if (_position < dataView.size()) {
    auto text = reinterpret_cast<const char*>(dataView.bytes() + _position);
    auto maxLength = dataView.size() - _position;
    auto textLength = strnlen(text, maxLength);
    if (textLength < maxLength) {
      positionChanged(textLength + 1);
      return {text, textLength};
    }
  }
  PAGThrowError(context, "End of file was encountered.");
  return "";
}

int32_t DecodeStream::readEncodedInt32() {
  auto data = readEncodedUint32();
  auto value = static_cast<int32_t>(data >> 1);
  return (data & 1) > 0 ? -value : value;
}

uint32_t DecodeStream::readEncodedUint32() {
  static const uint32_t valueMask = 127;
  static const uint8_t hasNext = 128;
  uint32_t value = 0;
  uint32_t byte = 0;
  auto length = dataView.size();
  auto bytes = dataView.bytes();
  for (int i = 0; i < 32; i += 7) {
    if (_position >= length) {
      PAGThrowError(context, "End of file was encountered.");
      break;
    }
    byte = bytes[_position++];
    value |= (byte & valueMask) << i;
    if ((byte & hasNext) == 0) {
      break;
    }
  }
  positionChanged(0);
  return value;
}

int64_t DecodeStream::readEncodedInt64() {
  auto data = readEncodedUint64();
  auto value = static_cast<int64_t>(data >> 1);
  return (data & 1) > 0 ? -value : value;
}

uint64_t DecodeStream::readEncodedUint64() {
  static const uint64_t valueMask = 127;
  static const uint8_t hasNext = 128;
  uint64_t value = 0;
  uint64_t byte = 0;
  auto length = dataView.size();
  auto bytes = dataView.bytes();
  for (int i = 0; i < 64; i += 7) {
    if (_position >= length) {
      PAGThrowError(context, "End of file was encountered.");
      break;
    }
    byte = bytes[_position++];
    value |= (byte & valueMask) << i;
    if ((byte & hasNext) == 0) {
      break;
    }
  }
  positionChanged(0);
  return value;
}

int32_t DecodeStream::readBits(uint8_t numBits) {
  auto value = readUBits(numBits);
  value <<= (32 - numBits);
  auto data = static_cast<int32_t>(value);
  return data >> (32 - numBits);
}

uint32_t DecodeStream::readUBits(uint8_t numBits) {
  static const uint8_t bitMasks[9] = {0, 1, 3, 7, 15, 31, 63, 127, 255};
  uint32_t value = 0;
  auto length = dataView.size();
  auto bytes = dataView.bytes();
  if ((length * 8 >= numBits) && (_bitPosition <= static_cast<uint64_t>(length) * 8 - numBits)) {
    uint32_t pos = 0;
    while (pos < numBits) {
      auto bytePosition = static_cast<uint32_t>(_bitPosition / 8);
      auto bitPosition = static_cast<uint32_t>(_bitPosition % 8);
      auto byte = bytes[bytePosition] >> bitPosition;
      auto bitLength = std::min(8 - bitPosition, numBits - pos);
      byte &= bitMasks[bitLength];
      value |= static_cast<uint32_t>(byte) << pos;
      pos += bitLength;
      _bitPosition += bitLength;
    }
    bitPositionChanged(0);
  } else {
    PAGThrowError(context, "End of file was encountered.");
  }
  return value;
}

void DecodeStream::readInt32List(int32_t* values, uint32_t count) {
  auto numBits = readNumBits();
  for (uint32_t i = 0; i < count; i++) {
    values[i] = readBits(numBits);
  }
}

void DecodeStream::readUint32List(uint32_t* values, uint32_t count) {
  auto numBits = readNumBits();
  for (uint32_t i = 0; i < count; i++) {
    values[i] = readUBits(numBits);
  }
}

void DecodeStream::readFloatList(float* values, uint32_t count, float precision) {
  auto numBits = readNumBits();
  for (uint32_t i = 0; i < count; i++) {
    values[i] = readBits(numBits) * precision;
  }
}
void DecodeStream::readPoint2DList(Point* points, uint32_t count, float precision) {
  auto numBits = readNumBits();
  for (uint32_t i = 0; i < count; i++) {
    points[i].x = readBits(numBits) * precision;
    points[i].y = readBits(numBits) * precision;
  }
}

void DecodeStream::readPoint3DList(Point3D* points, uint32_t count, float precision) {
  auto numBits = readNumBits();
  for (uint32_t i = 0; i < count; i++) {
    points[i].x = readBits(numBits) * precision;
    points[i].y = readBits(numBits) * precision;
    points[i].z = readBits(numBits) * precision;
  }
}

void DecodeStream::bitPositionChanged(size_t offset) {
  _bitPosition += offset;
  _position = BitsToBytes(_bitPosition);
}

void DecodeStream::positionChanged(size_t offset) {
  _position += offset;
  _bitPosition = _position * 8;
}

bool DecodeStream::checkEndOfFile(uint32_t bytesToRead) {
  // The _position must not use the uint32_t type, otherwise it will overflow.
  if (_position + bytesToRead > dataView.size()) {
    PAGThrowError(context, "End of file was encountered.");
    return true;
  }
  return false;
}
}  // namespace pag
