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

#include "DecodeStream.h"
#include <cstring>

namespace pag {

ByteOrder DecodeStream::order() const {
  return _order;
}

void DecodeStream::setOrder(pag::ByteOrder order) {
  _order = order;
}

void DecodeStream::setPosition(uint32_t value) {
  if (value < _length) {
    _position = value;
    positionChanged();
  } else {
    PAGThrowError(context, "End of file was encountered.");
  }
}

void DecodeStream::skip(uint32_t numBytes) {
  if (_position < _length - numBytes + 1) {
    _position += numBytes;
    positionChanged();
  } else {
    PAGThrowError(context, "End of file was encountered.");
  }
}

DecodeStream DecodeStream::readBytes(uint32_t length) {
  if ((_length >= length) && (_position <= _length - length)) {
    DecodeStream stream(context, bytes + _position, length);
    _position += length;
    positionChanged();
    return stream;
  } else {
    PAGThrowError(context, "End of file was encountered.");
  }
  return DecodeStream(context);
}

std::unique_ptr<ByteData> DecodeStream::readByteData() {
  auto length = readEncodedUint32();
  auto bytes = readBytes(length);
  // must check whether the bytes is valid. otherwise memcpy will crash.
  if (length == 0 || context->hasException()) {
    return nullptr;
  }
  return ByteData::MakeCopy(bytes.data(), length);
}

std::string DecodeStream::readUTF8String() {
  if (_position < _length) {
    auto text = reinterpret_cast<const char*>(bytes + _position);
    auto textLength = strlen(text);
    if (textLength > _length - _position) {
      textLength = _length - _position;
      _position += textLength;
    } else {
      _position += textLength + 1;
    }
    positionChanged();
    return std::string(text, textLength);
  } else {
    PAGThrowError(context, "End of file was encountered.");
  }
  return "";
}

int32_t DecodeStream::readEncodedInt32() {
  auto data = readEncodedUint32();
  int32_t value = data >> 1;
  return (data & 1) > 0 ? -value : value;
}

uint32_t DecodeStream::readEncodedUint32() {
  static const uint32_t valueMask = 127;
  static const uint8_t hasNext = 128;
  uint32_t value = 0;
  uint32_t byte = 0;
  for (int i = 0; i < 32; i += 7) {
    if (_position >= _length) {
      PAGThrowError(context, "End of file was encountered.");
      break;
    }
    byte = bytes[_position++];
    value |= (byte & valueMask) << i;
    if ((byte & hasNext) == 0) {
      break;
    }
  }
  positionChanged();
  return value;
}

int64_t DecodeStream::readEncodedInt64() {
  auto data = readEncodedUint64();
  int64_t value = data >> 1;
  return (data & 1) > 0 ? -value : value;
}

uint64_t DecodeStream::readEncodedUint64() {
  static const uint64_t valueMask = 127;
  static const uint8_t hasNext = 128;
  uint64_t value = 0;
  uint64_t byte = 0;
  for (int i = 0; i < 64; i += 7) {
    if (_position >= _length) {
      PAGThrowError(context, "End of file was encountered.");
      break;
    }
    byte = bytes[_position++];
    positionChanged();
    value |= (byte & valueMask) << i;
    if ((byte & hasNext) == 0) {
      break;
    }
  }
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
  if ((_length * 8 >= numBits) && (_bitPosition <= static_cast<uint64_t>(_length) * 8 - numBits)) {
    uint32_t pos = 0;
    while (pos < numBits) {
      auto bytePosition = static_cast<uint32_t>(_bitPosition * 0.125);
      auto bitPosition = static_cast<uint32_t>(_bitPosition % 8);
      auto byte = bytes[bytePosition] >> bitPosition;
      auto bitLength = std::min(8 - bitPosition, numBits - pos);
      byte &= bitMasks[bitLength];
      value |= static_cast<uint32_t>(byte) << pos;
      pos += bitLength;
      _bitPosition += bitLength;
    }
    bitPositionChanged();
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

Bit8 DecodeStream::readBit8() {
  Bit8 data = {};
  if (_position < _length) {
    data.uintValue = bytes[_position++];
    positionChanged();
  } else {
    PAGThrowError(context, "End of file was encountered.");
  }
  return data;
}

Bit16 DecodeStream::readBit16() {
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
  } else {
    PAGThrowError(context, "End of file was encountered.");
  }
  return data;
}

Bit32 DecodeStream::readBit24() {
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
  } else {
    PAGThrowError(context, "End of file was encountered.");
  }
  return data;
}

Bit32 DecodeStream::readBit32() {
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
  } else {
    PAGThrowError(context, "End of file was encountered.");
  }
  return data;
}

Bit64 DecodeStream::readBit64() {
  Bit64 data = {};
  if ((_length > 7) && (_position < _length - 7)) {
    if (_order == NATIVE_BYTE_ORDER) {
      for (int i = 0; i < 8; i++) {
        data.bytes[i] = bytes[_position++];
      }
    } else {
      for (int i = 7; i >= 0; i--) {
        data.bytes[i] = bytes[_position++];
      }
    }
    positionChanged();
  } else {
    PAGThrowError(context, "End of file was encountered.");
  }
  return data;
}
}  // namespace pag
