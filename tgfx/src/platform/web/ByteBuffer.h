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

#pragma once

#include <string>

namespace tgfx {

enum class ByteOrder { LittleEndian, BigEndian };

ByteOrder EndianTest();

static const ByteOrder NATIVE_BYTE_ORDER = EndianTest();

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

class ByteBuffer final {
 public:
  ByteBuffer(const uint8_t* data, uint32_t length) : bytes(data), _length(length) {
  }

  /**
   * Modifies this ByteBuffer's byte order.
   */
  void setOrder(ByteOrder order);

  const uint8_t* data() const {
    return bytes;
  }

  /**
   * The length of the ByteBuffer object.
   */
  uint32_t length() const {
    return _length;
  }

  /**
   * Moves, or returns the current position, of the file pointer into the DecodeStream object. This
   * is the point at which the next call to a read method starts reading.
   */
  uint32_t position() const {
    return _position;
  }

  void setPosition(uint32_t value);

  void skip(uint32_t numBytes);

  /**
   * Reads a unsigned 8-bit integer from the byte stream.
   */
  uint8_t readUint8() {
    return readBit8().uintValue;
  }

  /**
   * Reads a unsigned 16-bit integer from the byte stream.
   */
  uint16_t readUint16() {
    return readBit16().uintValue;
  }

  /**
   * Reads a unsigned 24-bit integer from the byte stream.
   */
  uint32_t readUint24() {
    return readBit24().uintValue;
  }

  /**
   * Reads a list of uint32_t values from the stream.
   */
  uint32_t readUint32() {
    return readBit32().uintValue;
  }

  std::string readString(size_t size) {
    auto str = std::string((char*)bytes + _position, size);
    _position += size;
    positionChanged();
    return str;
  }

 private:
  void positionChanged() {
    _bitPosition = static_cast<uint64_t>(_position) * 8;
  }

  Bit8 readBit8();
  Bit16 readBit16();
  Bit32 readBit24();
  Bit32 readBit32();

  ByteOrder _order = ByteOrder::LittleEndian;
  const uint8_t* bytes = nullptr;
  uint32_t _length = 0;
  uint32_t _position = 0;
  uint64_t _bitPosition = 0;
};
}  // namespace tgfx
